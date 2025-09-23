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
#include <sstream>
#include <cctype>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <unordered_map>
#include <vector>
#include <unordered_set>
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
static volatile std::sig_atomic_t g_userInitiated = 0;
static void handle_stop(int) { g_stop = 1; g_userInitiated = 1; }

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
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res = nullptr;
  int rc = ::getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
  if (rc != 0) {
    std::cerr << "getaddrinfo: " << gai_strerror(rc) << "\n";
    return -1;
  }
  constexpr int CONNECT_TIMEOUT_MS = 5000;
  int fd = -1;
  for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
    int tmp = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (tmp < 0)
      continue;
    int flags = ::fcntl(tmp, F_GETFL, 0);
    if (flags >= 0)
      ::fcntl(tmp, F_SETFL, flags | O_NONBLOCK);
    int r = ::connect(tmp, ai->ai_addr, ai->ai_addrlen);
    if (r == 0) {
      fd = tmp;                        // Connected immediately
      if (flags >= 0) ::fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
      break;
    } else if (r < 0 && errno == EINPROGRESS) {
      struct pollfd pfd{tmp, POLLOUT, 0};
      int pr = ::poll(&pfd, 1, CONNECT_TIMEOUT_MS);
      if (pr > 0 && (pfd.revents & POLLOUT)) {
        int err = 0;
        socklen_t len = sizeof(err);
        if (::getsockopt(tmp, SOL_SOCKET, SO_ERROR, &err, &len) == 0 && err == 0) {
          fd = tmp;
          if (flags >= 0) ::fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
          break;
        }
      }
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

static void send_quit(int fd, const std::string &reason) {
  if (fd < 0) return;
  send_line(fd, "QUIT :" + reason);
  ::close(fd);
  /* ::shutdown(fd, SHUT_WR); */
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
    int retries = 0;
    const int MAX_RETRIES = 5;
    std::unordered_set<std::string> joined_channels;
    std::unordered_set<std::string> filter_words;
    // Track channel operators (updated from 353 and MODE +o/-o)
    std::unordered_map<std::string, std::unordered_set<std::string>> channel_ops;
    // Last command sender (set when a bot command is processed)
    std::string last_sender_nick;

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

  commands["!filter"] = [&](const std::string &target, const std::string &args) {
    // Determine the channel context (only allow in a channel)
    if (target.empty() || target[0] != '#') {
      reply((target == nick ? last_sender_nick : target), "Filter can only be modified from a channel context");
      return;
    }
    // Require that the sender is a known channel operator
    auto chanIt = channel_ops.find(target);
    if (chanIt == channel_ops.end() || chanIt->second.find(last_sender_nick) == chanIt->second.end()) {
      reply(target, "You are not a channel operator - cannot modify filter");
      return;
    }
    std::istringstream iss(args);
    std::string w;
    int added = 0;
    while (iss >> w) {
      std::string lw;
      lw.reserve(w.size());
      for (char c : w)
        lw.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
      if (!lw.empty()) {
        if (filter_words.insert(lw).second)
          ++added;
      }
    }
    reply(target, "Filter updated: " + std::to_string(filter_words.size()) +
                  " words total (" + std::to_string(added) + " new)");
  };

  while (!g_stop) {
    sockfd = connect_to(host, port);
    if (sockfd < 0) {
      ++retries;
      std::cerr << "[bot] connect failed (" << retries << "/" << MAX_RETRIES << "), retry in " << backoff << "s\n";
      if (retries >= MAX_RETRIES) {
        std::cerr << "[bot] maximum retries reached, exiting\n";
        break;
      }
      for (int s = 0; s < backoff && !g_stop; ++s)
        sleep(1);
      backoff = std::min(backoff_max, backoff * 2);
      continue;
    }
    std::cerr << "[bot] connected\n";
    retries = 0;
    backoff = 2;

    if (!pass.empty())
      send_line(sockfd, "PASS " + pass);
    send_line(sockfd, "NICK " + nick);
    send_line(sockfd, "USER " + nick + " 0 * :irc_hive bot");

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
        std::cerr << "[bot] socket closed (treating as server shutdown, exiting)\n";
        g_stop = 1;
        break;
      }
      if (pfd.revents & POLLIN) {
        char buf[4096];
        ssize_t n = ::recv(sockfd, buf, sizeof(buf), 0);
        if (n <= 0) {
          std::cerr << "[bot] recv <= 0 (treating as server shutdown, exiting)\n";
		  ::close(sockfd);
          g_stop = 1;
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

          // Update channel operators from RPL_NAMREPLY (353)
          // Typical: 353 <ourNick> = #channel :@Op1 +VoicedUser User2
          if (msg->command == "353" && msg->params.size() >= 4) {
              const std::string &chan = msg->params[2];
              const std::string &namesField = msg->params[3];
              // namesField may already be without leading ':' depending on parser
              std::string names = namesField;
              if (!names.empty() && names[0] == ':')
                  names.erase(0, 1);
              std::istringstream nss(names);
              std::string token;
              auto &ops = channel_ops[chan]; // creates if not present
              while (nss >> token) {
                  if (!token.empty() && (token[0] == '@')) {
                      std::string opNick = token.substr(1);
                      if (!opNick.empty())
                          ops.insert(opNick);
                  }
              }
          }

          // Track MODE +o / -o changes: :setter MODE #channel +o nick  OR grouped like +oo nick1 nick2
          if (msg->command == "MODE" && msg->params.size() >= 2) {
              const std::string &chan = msg->params[0];
              if (!chan.empty() && chan[0] == '#') {
                  const std::string &modes = msg->params[1];
                  // Remaining params are the nick targets consumed by 'o' (and others we ignore)
                  std::vector<std::string> modeArgs;
                  for (size_t i = 2; i < msg->params.size(); ++i)
                      modeArgs.push_back(msg->params[i]);

                  bool adding = true;
                  size_t argIndex = 0;
                  auto &ops = channel_ops[chan]; // creates if missing
                  for (char c : modes) {
                      if (c == '+') { adding = true; continue; }
                      if (c == '-') { adding = false; continue; }
                      if (c == 'o') {
                          if (argIndex < modeArgs.size()) {
                              const std::string &targetNick = modeArgs[argIndex++];
                              if (!targetNick.empty()) {
                                  if (adding)
                                      ops.insert(targetNick);
                                  else
                                      ops.erase(targetNick);
                              }
                          }
                      } else {
                          // For other mode letters that take arguments, still consume if needed
                          // but since we only care about 'o', we just attempt to advance if letter uses an argument.
                          // Simple heuristic: ignore; most other user modes won't break ordering here for small scale.
                      }
                  }
              }
          }

          // PRIVMSG handling (commands + filter enforcement)
                    if (msg->command == "PRIVMSG" && msg->params.size() >= 2) {
                      const std::string &target = msg->params[0];
                      const std::string &text = msg->params[1];
                      std::string sender_nick = extract_nick_from_prefix(msg->prefix);

                      // Bot commands (prefix '!')
                      if (!text.empty() && text[0] == '!') {
                        size_t spc = text.find(' ');
                        std::string key  = (spc == std::string::npos) ? text : text.substr(0, spc);
                        std::string args = (spc == std::string::npos) ? ""   : text.substr(spc + 1);
                        auto it = commands.find(key);
                        if (it != commands.end()) {
                          std::string reply_target =
                              (target == nick && !sender_nick.empty()) ? sender_nick : target;
                          // Record sender for privilege checks (e.g. !filter)
                          last_sender_nick = sender_nick;
                          it->second(reply_target, args);
                        }
                      }

                      // Content filter (only for channel messages, not from the bot itself)
                      if (!filter_words.empty() && !sender_nick.empty() && sender_nick != nick &&
                          !target.empty() && target[0] == '#' &&
                          (text.empty() || text[0] != '!')) {
                        // Tokenize text into words (alnum sequences)
                        std::string token;
                        auto flush_token = [&](void) {
                          if (token.empty()) return false;
                          if (filter_words.find(token) != filter_words.end()) {
                            send_line(sockfd, "KICK " + target + " " + sender_nick + " :filtered word");
                            return true;
                          }
                          token.clear();
                          return false;
                        };
                        for (char ch : text) {
                          if (std::isalnum(static_cast<unsigned char>(ch))) {
                            token.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
                          } else {
                            if (flush_token())
                              break;
                          }
                        }
                        if (!token.empty())
                          flush_token();
                      }
                    }

                    // Track successful self JOIN to mark channel as joined
                    if (msg->command == "JOIN" && msg->params.size() >= 1) {
                        std::string selfNick = extract_nick_from_prefix(msg->prefix);
                        if (selfNick == nick) {
                            const std::string &chan = msg->params[0];
                            if (!chan.empty())
                                joined_channels.insert(chan);
                        }
                    }

                     if (msg->command == "QUIT") {
                         std::string quitter = extract_nick_from_prefix(msg->prefix);
                         for (auto &pair : channel_ops) {
                             pair.second.erase(quitter);
                         }
                     }

                     if (msg->command == "PART" && msg->params.size() >= 1) {
                         std::string parter = extract_nick_from_prefix(msg->prefix);
                         const std::string &chan = msg->params[0];
                         auto it = channel_ops.find(chan);
                         if (it != channel_ops.end()) {
                             it->second.erase(parter);
                         }
                     }

                    // INVITE handling: always attempt (re)JOIN if not already joined
                    if (msg->command == "INVITE" && msg->params.size() >= 2) {
                        const std::string &invitee = msg->params[0];
                        const std::string &channel = msg->params[1];
                        if (invitee == nick && !channel.empty()) {
                            // Track desired channel list (only add once)
                            if (std::find(channels.begin(), channels.end(), channel) == channels.end()) {
                                channels.push_back(channel);
                            }
                            // Re-attempt join if we are not yet in joined set
                            if (joined_channels.find(channel) == joined_channels.end()) {
                                send_line(sockfd, "JOIN " + channel);
                            }
                        }
                    }

                    msg_queue.pop();
        }
      }
    }

    if (g_stop && g_userInitiated) {
      send_quit(sockfd, "Client exiting");
      struct pollfd tmp{sockfd, POLLIN, 0};
      poll(&tmp, 1, 100);
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
