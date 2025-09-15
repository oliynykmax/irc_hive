#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <poll.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>

/* Signal handling */
volatile sig_atomic_t g_signal_received = 0;
void signal_handler(int sig) { g_signal_received = sig; }

/* Embedded minimal AI HTTP code (curl shell-out) */
static const char AI_HOST[] = "api.deepseek.com";
static const char AI_PATH[] = "/v1/chat/completions";
static const char AI_KEY[] = "...";

static std::string jsonEscape(const std::string &in) {
  std::string out;
  out.reserve(in.size() + 16);
  for (char c : in) {
    switch (c) {
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\"";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out += c;
      break;
    }
  }
  return out;
}

static std::string aiHttpPost(const std::string &payloadJsonEscapedForShell) {
  std::ostringstream cmd;
  cmd << "curl -sS -X POST https://" << AI_HOST << AI_PATH
      << " -H 'Authorization: Bearer " << AI_KEY << "'"
      << " -H 'Content-Type: application/json'"
      << " -H 'Accept: application/json'"
      << " --fail"
      << " -d '" << payloadJsonEscapedForShell << "'";
  FILE *pipe = popen(cmd.str().c_str(), "r");
  if (!pipe) {
    return "Error: cannot start curl";
  }
  std::string response;
  char buf[4096];
  while (fgets(buf, sizeof(buf), pipe)) {
    response.append(buf);
  }
  int rc = pclose(pipe);
  if (rc != 0) {
    std::ostringstream err;
    err << "Error: curl exit " << rc;
    return err.str();
  }
  return response;
}

static std::string parseAI(const std::string &response) {
  std::string extracted;
  size_t searchStart = response.find("\"role\":\"assistant\"");
  if (searchStart == std::string::npos)
    searchStart = 0;
  size_t keyPos = response.find("\"content\"", searchStart);
  if (keyPos != std::string::npos) {
    size_t colon = response.find(':', keyPos);
    if (colon != std::string::npos) {
      size_t firstQuote = response.find('"', colon);
      if (firstQuote != std::string::npos) {
        size_t i = firstQuote + 1;
        while (i < response.size()) {
          char c = response[i];
          if (c == '\\' && i + 1 < response.size()) {
            char n = response[i + 1];
            if (n == 'n')
              extracted.push_back('\n');
            else if (n == 'r') { /* skip */
            } else
              extracted.push_back(n);
            i += 2;
            continue;
          }
          if (c == '"')
            break;
          extracted.push_back(c);
          ++i;
        }
      }
    }
  }
  if (extracted.empty())
    return response;
  if (extracted.size() > 400) {
    extracted.erase(400);
  }
  return extracted;
}

static std::string buildAIPayload(const std::string &userPrompt) {
  static const std::string systemPrompt =
      "You are a concise chatbot for IRC. Reply casually, sometimes lazy. "
      "Never exceed 400 characters.";
  std::string json = "{\"model\":\"deepseek-chat\","
                     "\"messages\":["
                     "{\"role\":\"system\",\"content\":\"" +
                     jsonEscape(systemPrompt) +
                     "\"},"
                     "{\"role\":\"user\",\"content\":\"" +
                     jsonEscape(userPrompt) +
                     "\"}"
                     "],"
                     "\"temperature\":0.7}";
  std::string shellEscaped;
  shellEscaped.reserve(json.size() + 32);
  for (char c : json) {
    if (c == '\'') {
      shellEscaped += "'\\''";
    } else {
      shellEscaped += c;
    }
  }
  return shellEscaped;
}

static std::string callAI(const std::string &prompt) {
  std::string payload = buildAIPayload(prompt);
  std::string raw = aiHttpPost(payload);
  return parseAI(raw);
}

static bool sendLine(int fd, const std::string &line) {
  std::string wire = line + "\r\n";
  size_t sent = 0;
  while (sent < wire.size()) {
    ssize_t n = send(fd, wire.data() + sent, wire.size() - sent, 0);
    if (n <= 0)
      return false;
    sent += (size_t)n;
  }
  return true;
}

int main(int argc, char *argv[]) {
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  if (argc < 5) {
    std::cerr << "Usage: " << argv[0]
              << " <host> <port> <nick> <#channel> [password]" << std::endl;
    return 1;
  }
  std::string host = argv[1];
  std::string port = argv[2];
  std::string nick = argv[3];
  std::string channel = argv[4];
  std::string password = (argc >= 6) ? argv[5] : "";

  struct addrinfo hints{}, *res = nullptr;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;
  if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0 || !res) {
    std::cerr << "DNS resolution failed" << std::endl;
    return 1;
  }
  int sock = -1;
  for (auto p = res; p; p = p->ai_next) {
    sock = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sock < 0)
      continue;
    
    // Make socket non-blocking for connect with timeout
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    int connect_result = ::connect(sock, p->ai_addr, p->ai_addrlen);
    if (connect_result == 0) {
      // Connection succeeded immediately
      break;
    } else if (errno == EINPROGRESS) {
      // Connection in progress, use poll to wait with timeout
      struct pollfd pfd;
      pfd.fd = sock;
      pfd.events = POLLOUT;
      
      int poll_result = poll(&pfd, 1, 5000); // 5 second timeout
      if (poll_result > 0 && (pfd.revents & POLLOUT)) {
        // Connection completed
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0) {
          break;
        }
      }
    }
    
    close(sock);
    sock = -1;
  }
  freeaddrinfo(res);
  if (sock < 0) {
    std::cerr << "Connection failed" << std::endl;
    return 1;
  }
  
  // Set socket back to blocking mode
  int flags = fcntl(sock, F_GETFL, 0);
  fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);

  if (!password.empty())
    sendLine(sock, "PASS " + password);
  sendLine(sock, "NICK " + nick);
  sendLine(sock, "USER " + nick + " 0 * :" + nick);
  sendLine(sock, "JOIN " + channel);

  std::string buffer;
  buffer.reserve(8192);

  struct pollfd pfd;
  pfd.fd = sock;
  pfd.events = POLLIN;
  bool shouldExit = false;

  while (!shouldExit && g_signal_received == 0) {
    int ret = poll(&pfd, 1, 1000); // 1s timeout for responsiveness
    if (ret < 0) {
      if (errno == EINTR) {
        if (g_signal_received != 0)
          break; // Signal received, exit loop
        continue; // Interrupted by signal but no exit signal, loop again
      }
      perror("poll");
      break;
    }
    if (ret == 0) {
      continue; // Timeout, just loop again
    }
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
      std::cerr << "Connection error." << std::endl;
      break;
    }
    if (pfd.revents & POLLIN) {
      char temp[2048];
      ssize_t n = recv(sock, temp, sizeof(temp), 0);
      if (n <= 0) {
        if (n < 0)
          perror("recv");
        else
          std::cout << "Connection closed." << std::endl;
        break;
      }
      buffer.append(temp, n);
    }

    size_t pos;
    while (!shouldExit && (pos = buffer.find("\r\n")) != std::string::npos) {
      std::string line = buffer.substr(0, pos);
      buffer.erase(0, pos + 2);

      if (line.rfind("PING", 0) == 0) {
        sendLine(sock, "PONG " + line.substr(5));
        continue;
      }

      size_t priv = line.find(" PRIVMSG ");
      if (priv != std::string::npos) {
        size_t msgStart = line.find(" :", priv);
        if (msgStart != std::string::npos) {
          std::string senderNick;
          if (line.size() > 1) {
            size_t excl = line.find('!');
            if (excl != std::string::npos && excl > 1)
              senderNick = line.substr(1, excl - 1);
          }

          std::string target = line.substr(priv + 9, msgStart - (priv + 9));
          std::string text = line.substr(msgStart + 2);
          bool isOwn = (!senderNick.empty() && senderNick == nick);

          if ((target == channel && !isOwn) || (target == nick && !isOwn)) {
            std::string prompt = text;
            if (prompt.find(nick) != std::string::npos) {
              size_t p;
              while ((p = prompt.find(nick)) != std::string::npos) {
                prompt.erase(p, nick.size());
              }
            }
            while (!prompt.empty() && isspace((unsigned char)prompt.front()))
              prompt.erase(prompt.begin());
            while (!prompt.empty() && isspace((unsigned char)prompt.back()))
              prompt.pop_back();

            if (prompt == "!quit") {
              std::string quitReplyTo = (target == nick) ? senderNick : channel;
              sendLine(sock, "PRIVMSG " + quitReplyTo + " :Bye!");
              shouldExit = true;
              break;
            }

            std::string aiOut = callAI(prompt);
            if (aiOut.size() > 400)
              aiOut.erase(400);
            std::stringstream reply;
            std::string replyTo = (target == nick) ? senderNick : channel;
            reply << "PRIVMSG " << replyTo << " :" << aiOut;
            sendLine(sock, reply.str());
          }
        }
      }
    }
  }

  if (g_signal_received != 0) {
    std::cout << "\nCaught signal " << g_signal_received << ", exiting."
              << std::endl;
  }

  close(sock);
  return 0;
}
