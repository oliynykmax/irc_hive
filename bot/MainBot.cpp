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
#include "../inc/RecvParser.hpp"

inline constexpr std::array<std::string_view, 9> HELP_LINES = {
    "/part {message} - leave the channel, with a message",
    "/kick [nick] {message} - kick the user out of the channel, with a message",
    "/quit {message} - disconnect from the server, with a message",
    "/mode [l/o/t/k/i] - sets the channel modes, i-invite mode, o-operator, t-topic, l-limit, k-key",
    "/msg [nick] - send message to user",
    "/topic {name} - sets the topic of the channel to {name}",
    "/invite [nick] {#channel_name} - invites user to the channel",
    "/join {#channel_name} - joins the channel",
    "/nick [nick] - tries to change the nick"
};



static volatile std::sig_atomic_t g_stop = 0;
static void handle_stop(int) { g_stop = 1; }

static void fatal_usage() {
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
        fatal_usage();
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
      size_t start = 0;
      while (true) {
        size_t pos = chs.find(',', start);
        std::string token = (pos == std::string::npos)
                              ? chs.substr(start)
                              : chs.substr(start, pos - start);
        if (!token.empty())
          channels.push_back(token);
        if (pos == std::string::npos)
          break;
        start = pos + 1;
      }
    } else if (a == "--pass")
      need(pass);
    else {
      std::cerr << "Unknown arg: " << a << "\n";
      fatal_usage();
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

static std::string extract_nick_from_prefix(const std::optional<std::string> &prefix) {
  if (!prefix || prefix->empty())
    return "";
  size_t excl = prefix->find('!');
  return (excl == std::string::npos) ? *prefix : prefix->substr(0, excl);
}

static void log_message(const Message &msg) {
  std::cerr << "<< ";
  if (msg.prefix)
    std::cerr << ":" << *msg.prefix << " ";
  std::cerr << msg.command;
  for (size_t i = 0; i < msg.params.size(); ++i) {
    bool is_last = (i == msg.params.size() - 1);
    if (is_last && msg.params[i].find(' ') != std::string::npos)
      std::cerr << " :" << msg.params[i];
    else
      std::cerr << " " << msg.params[i];
  }
  std::cerr << "\n";
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

  commands["!help"] = [&](const std::string &target, const std::string &args) {
    (void)args;
    for (std::string_view line : HELP_LINES) {
      reply(target, std::string(line));
    }
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
    send_line(sockfd, "USER " + nick + " localhost * :irc_hive bot");

    bool joined = false;

    std::queue<std::unique_ptr<Message>> msg_queue;
    RecvParser parser(msg_queue);

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
        parser.feed(buf, static_cast<size_t>(n));

        while (!msg_queue.empty()) {
          std::unique_ptr<Message> &msg = msg_queue.front();

          log_message(*msg);

            // PING
          if (msg->command == "PING") {
            std::string token = msg->params.empty() ? "" : msg->params[0];
            send_line(sockfd, "PONG :" + token);
            msg_queue.pop();
            continue;
          }

          // Welcome / end of MOTD / no MOTD
          if (msg->command == "001" || msg->command == "376" || msg->command == "422") {
            if (!joined && !channels.empty()) {
              join_channels(sockfd, channels);
              joined = true;
            }
            msg_queue.pop();
            continue;
          }

          // Nick in use
          if (msg->command == "433") {
            ++nick_attempt;
            nick = base_nick + "_" + std::to_string(nick_attempt);
            std::cerr << "[bot] nick in use, trying " << nick << "\n";
            send_line(sockfd, "NICK " + nick);
            msg_queue.pop();
            continue;
          }

          // PRIVMSG handling
          if (msg->command == "PRIVMSG" && msg->params.size() >= 2) {
            const std::string &target = msg->params[0];
            const std::string &text = msg->params[1];
            if (!text.empty() && text[0] == '!') {
              size_t spc = text.find(' ');
              std::string key  = (spc == std::string::npos) ? text : text.substr(0, spc);
              std::string args = (spc == std::string::npos) ? ""   : text.substr(spc + 1);
              auto it = commands.find(key);
              if (it != commands.end()) {
                std::string sender_nick = extract_nick_from_prefix(msg->prefix);
                std::string reply_target =
                    (target == nick && !sender_nick.empty()) ? sender_nick : target;
                it->second(reply_target, args);
              }
            }
          }

          msg_queue.pop();
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
