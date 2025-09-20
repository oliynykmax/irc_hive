#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <netdb.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

static volatile std::sig_atomic_t g_stop = 0;
static void handle_stop(int) { g_stop = 1; }

static std::string trim_crlf(std::string s) {
  while (!s.empty() && (s.back() == '\r' || s.back() == '\n'))
    s.pop_back();
  return s;
}

static std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == delim) {
      out.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  out.push_back(cur);
  return out;
}

static void usage() {
  std::cerr << "Usage: ./ircbot [options]\n"
            << "  -s HOST      IRC server (default: localhost)\n"
            << "  -p PORT      IRC port (default: 6667)\n"
            << "  -n NICK      Nickname (default: ircbot)\n"
            << "  -c CHS       Comma-separated channels, e.g. \"#test,#bots\"\n"
            << "  --pass PASS  Server password\n";
  std::exit(2);
}

static void parse_args(int argc, char **argv, std::string &host,
                       std::string &port, std::string &nick,
                       std::vector<std::string> &channels, std::string &pass) {
  host = "localhost";
  port = "6667";
  nick = "ircbot";
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto need = [&](std::string &out) {
      if (i + 1 >= argc) {
        usage();
      }
      out = argv[++i];
    };
    if (a == "-s")
      need(host);
    else if (a == "-p")
      need(port);
    else if (a == "-n")
      need(nick);
    else if (a == "-c") {
      std::string chs;
      need(chs);
      for (std::string c : split(chs, ','))
        if (!c.empty())
          channels.push_back(c);
    } else if (a == "--pass")
      need(pass);
    else {
      std::cerr << "Unknown arg: " << a << "\n";
      usage();
    }
  }
}

static int connect_to(const std::string &host, const std::string &port) {
  struct addrinfo hints;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res = nullptr;
  int rc = ::getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
  if (rc != 0) {
    std::cerr << "getaddrinfo: " << gai_strerror(rc) << "\n";
    return -1;
  }
  int fd = -1;
  for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
    int tmp = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (tmp < 0)
      continue;
    if (::connect(tmp, ai->ai_addr, ai->ai_addrlen) == 0) {
      fd = tmp;
      break;
    }
    ::close(tmp);
  }
  ::freeaddrinfo(res);
  return fd;
}

static bool send_line(int fd, const std::string &s) {
  std::cerr << ">> " << s << "\n";
  std::string wire = s;
  wire += "\r\n";
  const char *p = wire.c_str();
  size_t left = wire.size();
  while (left > 0) {
    ssize_t n = ::send(fd, p, left, MSG_NOSIGNAL);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      std::cerr << "send failed: " << std::strerror(errno) << "\n";
      return false;
    }
    left -= static_cast<size_t>(n);
    p += n;
  }
  return true;
}

static bool send_privmsg(int fd, const std::string &target,
                         const std::string &text) {
  return send_line(fd, "PRIVMSG " + target + " :" + text);
}

static void join_channels(int fd, const std::vector<std::string> &channels) {
  for (const auto &ch : channels)
    if (!ch.empty())
      send_line(fd, "JOIN " + ch);
}

int main(int argc, char **argv) {
  std::signal(SIGINT, handle_stop);
  std::signal(SIGTERM, handle_stop);
  std::signal(SIGPIPE, SIG_IGN);

  std::string host, port, nick, pass;
  std::vector<std::string> channels;
  parse_args(argc, argv, host, port, nick, channels, pass);

  const std::string base_nick = nick;
  int nick_attempt = 0;
  int backoff = 2, backoff_max = 30;

  using Cmd = std::function<void(const std::string &, const std::string &)>;
  std::unordered_map<std::string, Cmd> commands;

  int sockfd = -1;
  auto reply = [&](const std::string &target, const std::string &text) {
    if (sockfd >= 0)
      send_privmsg(sockfd, target, text);
  };
  commands["!ping"] = [&](const std::string &target, const std::string &args) {
    (void)args;
    reply(target, "pong!");
  };

  while (!g_stop) {
    sockfd = connect_to(host, port);
    if (sockfd < 0) {
      std::cerr << "[bot] connect failed, retry in " << backoff << "s\n";
      for (int s = 0; s < backoff && !g_stop; ++s)
        sleep(1);
      backoff = std::min(backoff_max, backoff * 2);
      continue;
    }
    std::cerr << "[bot] connected\n";
    backoff = 2;

    if (!pass.empty())
      send_line(sockfd, "PASS " + pass);
    send_line(sockfd, "NICK " + nick);
    send_line(sockfd, "USER " + nick + " 0 * :irc_hive bot");

    bool joined = false;
    std::string inbuf;

    while (!g_stop) {
      struct pollfd pfd{sockfd, POLLIN, 0};
      int pr = ::poll(&pfd, 1, 10000);
      if (pr < 0) {
        if (errno == EINTR)
          continue;
        std::cerr << "poll error: " << std::strerror(errno) << "\n";
        break;
      }
      if (pr == 0)
        continue;
      if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        std::cerr << "[bot] socket closed\n";
        break;
      }
      if (pfd.revents & POLLIN) {
        char buf[4096];
        ssize_t n = ::recv(sockfd, buf, sizeof(buf), 0);
        if (n <= 0) {
          std::cerr << "[bot] recv <= 0\n";
          break;
        }
        inbuf.append(buf, buf + n);

        for (;;) {
          size_t pos = inbuf.find('\n');
          if (pos == std::string::npos)
            break;
          std::string line = trim_crlf(inbuf.substr(0, pos + 1));
          inbuf.erase(0, pos + 1);
          if (line.empty())
            continue;
          std::cerr << "<< " << line << "\n";

          if (line.rfind("PING", 0) == 0) {
            std::string token;
            size_t colon = line.find(':');
            if (colon != std::string::npos)
              token = line.substr(colon + 1);
            send_line(sockfd, "PONG :" + token);
            continue;
          }

          std::string s = line;
          std::string prefix, sender_nick;
          if (!s.empty() && s[0] == ':') {
            size_t sp = s.find(' ');
            if (sp != std::string::npos) {
              prefix = s.substr(1, sp - 1);
              s.erase(0, sp + 1);
              size_t excl = prefix.find('!');
              sender_nick =
                  (excl != std::string::npos) ? prefix.substr(0, excl) : prefix;
            } else
              s.erase(0, 1);
          }

          std::string trailing;
          size_t tpos = s.find(" :");
          if (tpos != std::string::npos) {
            trailing = s.substr(tpos + 2);
            s.erase(tpos);
          }

          std::vector<std::string> parts = split(s, ' ');
          if (parts.empty())
            continue;
          const std::string &cmd = parts[0];

          if (cmd == "001" || cmd == "376" || cmd == "422") {
            if (!joined && !channels.empty()) {
              join_channels(sockfd, channels);
              joined = true;
            }
            continue;
          }
          if (cmd == "433") {
            ++nick_attempt;
            nick = base_nick + "_" + std::to_string(nick_attempt);
            std::cerr << "[bot] nick in use, trying " << nick << "\n";
            send_line(sockfd, "NICK " + nick);
            continue;
          }
          if (cmd == "PRIVMSG" && parts.size() >= 2) {
            const std::string &target = parts[1];
            const std::string &text = trailing;
            if (!text.empty() && text[0] == '!') {
              size_t spc = text.find(' ');
              std::string key =
                  (spc == std::string::npos) ? text : text.substr(0, spc);
              std::string args =
                  (spc == std::string::npos) ? "" : text.substr(spc + 1);
              auto it = commands.find(key);
              if (it != commands.end()) {
                std::string reply_target =
                    (target == nick && !sender_nick.empty()) ? sender_nick
                                                             : target;
                it->second(reply_target, args);
              }
            }
          }
        }
      }
    }

    ::close(sockfd);
    sockfd = -1;
    if (g_stop)
      break;

    std::cerr << "[bot] reconnecting in " << backoff << "s\n";
    for (int s = 0; s < backoff && !g_stop; ++s)
      sleep(1);
    backoff = std::min(backoff_max, backoff * 2);
    nick = base_nick;
    nick_attempt = 0;
  }
  return 0;
}
