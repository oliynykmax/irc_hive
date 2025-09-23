#include "Bot.hpp"
#include "../inc/RecvParser.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <poll.h>
#include <sstream>
#include <cctype>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

static volatile std::sig_atomic_t g_stop = 0;
static volatile std::sig_atomic_t g_userInitiated = 0;
static void handle_stop(int) { g_stop = 1; g_userInitiated = 1; }

Bot::Bot(const Config &c)
: sockfd(-1), cfg(c), currentNick(cfg.nick), baseNick(cfg.nick), autoJoined(false),
  nick_attempt(0), backoff(2), backoff_max(30), retries(0) {
    setup_builtin_commands();
}

Bot::~Bot() {
    if (sockfd >= 0) ::close(sockfd);
}

// ---------------- Public API ----------------
void Bot::add_command(const std::string &key, std::function<void(const std::string &, const std::string &)> fn) {
    commands[key] = fn;
}


int Bot::connect_socket() {
    struct addrinfo hints; std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res = nullptr;
    int rc = ::getaddrinfo(cfg.host.c_str(), cfg.port.c_str(), &hints, &res);
    if (rc != 0) { std::cerr << "getaddrinfo: " << gai_strerror(rc) << "\n"; return -1; }
    constexpr int CONNECT_TIMEOUT_MS = 5000;
    int fd = -1;
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        int tmp = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (tmp < 0) continue;
        int flags = ::fcntl(tmp, F_GETFL, 0);
        if (flags >= 0) ::fcntl(tmp, F_SETFL, flags | O_NONBLOCK);
        int r = ::connect(tmp, ai->ai_addr, ai->ai_addrlen);
        if (r == 0) { fd = tmp; if (flags >= 0) ::fcntl(fd, F_SETFL, flags & ~O_NONBLOCK); break; }
        else if (r < 0 && errno == EINPROGRESS) {
            struct pollfd pfd{tmp, POLLOUT, 0};
            int pr = ::poll(&pfd, 1, CONNECT_TIMEOUT_MS);
            if (pr > 0 && (pfd.revents & POLLOUT)) {
                int err = 0; socklen_t len = sizeof(err);
                if (::getsockopt(tmp, SOL_SOCKET, SO_ERROR, &err, &len) == 0 && err == 0) {
                    fd = tmp; if (flags >= 0) ::fcntl(fd, F_SETFL, flags & ~O_NONBLOCK); break;
                }
            }
        }
        ::close(tmp);
    }
    ::freeaddrinfo(res);
    return fd;
}

void Bot::login() {
    if (!cfg.pass.empty()) send_line("PASS " + cfg.pass);
    send_line("NICK " + currentNick);
    send_line("USER " + currentNick + " 0 * :irc_hive bot");
}

void Bot::send_line(const std::string &s) {
    std::cerr << ">> " << s << "\n";
    std::string wire = s + "\r\n";
    const char *p = wire.c_str(); size_t left = wire.size();
    while (left > 0) {
        ssize_t n = ::send(sockfd, p, left, MSG_NOSIGNAL);
        if (n < 0) { if (errno == EINTR) continue; std::cerr << "send failed: " << std::strerror(errno) << "\n"; return; }
        left -= static_cast<size_t>(n); p += n;
    }
}

void Bot::send_privmsg(const std::string &target, const std::string &text) { send_line("PRIVMSG " + target + " :" + text); }

void Bot::join_channels() { for (const auto &ch : cfg.channels) if (!ch.empty()) send_line("JOIN " + ch); }

std::string Bot::extract_nick_from_prefix(const std::optional<std::string> &prefix) {
    if (!prefix || prefix->empty()) {
        return "";
    }
    size_t excl = prefix->find('!');
    return (excl == std::string::npos) ? *prefix : prefix->substr(0, excl);
}

void Bot::log_message(const Message &msg) {
    std::cerr << "<< ";
    if (msg.prefix) std::cerr << ":" << *msg.prefix << " ";
    std::cerr << msg.command;
    for (size_t i = 0; i < msg.params.size(); ++i) {
        bool is_last = (i == msg.params.size() - 1);
        if (is_last && msg.params[i].find(' ') != std::string::npos) std::cerr << " :" << msg.params[i];
        else std::cerr << " " << msg.params[i];
    }
    std::cerr << "\n";
}

void Bot::setup_builtin_commands() {
    auto reply = [&](const std::string &target, const std::string &text) { if (sockfd >= 0) send_privmsg(target, text); };

    add_command("!ping", [&, reply](const std::string &target, const std::string &args){ (void)args; reply(target, "pong!"); });

    add_command("!help", [&, reply](const std::string &target, const std::string &args){ (void)args;
        reply(target, "/part {message} - leave the channel, with a message");
        reply(target, "/kick [nick] {message} - kick the user out of the channel, with a message");
        reply(target, "/quit {message} - disconnect from the server, with a message");
        reply(target, "/mode [l/o/t/k/i] - sets the channel modes, i-invite mode, o-operator, t-topic, l-limit, k-key");
        reply(target, "/msg [nick] - send message to user");
        reply(target, "/topic {name} - sets the topic of the channel to {name}");
        reply(target, "/invite [nick] {#channel_name} - invites user to the channel");
        reply(target, "/join {#channel_name} - joins the channel");
        reply(target, "/nick [nick] - tries to change the nick");
        reply(target, "!filter word ... - add words to channel filter");
        reply(target, "!unfilter word ... - remove words from channel filter");
        reply(target, "!listfilter - list channel filter words");
    });

    add_command("!filter", [&, reply](const std::string &target, const std::string &args){
        if (target.empty() || target[0] != '#') {
            std::string reply_target = (target == currentNick && !last_sender_nick.empty()) ? last_sender_nick : target;
            reply(reply_target, "Filter can only be modified from a channel context");
            return;
        }
        auto chanIt = channel_ops.find(target);
        if (chanIt == channel_ops.end() || chanIt->second.find(last_sender_nick) == chanIt->second.end()) {
            reply(target, "You are not a channel operator - cannot modify filter");
            return;
        }
        if (args.empty()) { reply(target, "Usage: !filter word [word...]"); return; }
        std::istringstream iss(args); std::string w; int added = 0;
        auto &filter = channel_filters[target];
        while (iss >> w) {
            std::string lw; lw.reserve(w.size());
            for (char c : w) lw.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            if (!lw.empty()) if (filter.insert(lw).second) ++added;
        }
        reply(target, "Filter updated: " + std::to_string(filter.size()) + " words total (" + std::to_string(added) + " new)");
    });

    add_command("!unfilter", [&, reply](const std::string &target, const std::string &args){
        if (target.empty() || target[0] != '#') {
            std::string reply_target = (target == currentNick && !last_sender_nick.empty()) ? last_sender_nick : target;
            reply(reply_target, "Filter can only be modified from a channel context");
            return;
        }
        auto chanIt = channel_ops.find(target);
        if (chanIt == channel_ops.end() || chanIt->second.find(last_sender_nick) == chanIt->second.end()) {
            reply(target, "You are not a channel operator - cannot modify filter");
            return;
        }
        if (args.empty()) { reply(target, "Usage: !unfilter word [word...]"); return; }
        auto fit = channel_filters.find(target);
        if (fit == channel_filters.end() || fit->second.empty()) { reply(target, "Filter empty"); return; }
        std::istringstream iss(args); std::string w; int removed = 0; auto &filter = fit->second;
        while (iss >> w) {
            std::string lw; lw.reserve(w.size());
            for (char c : w) lw.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            if (!lw.empty()) removed += filter.erase(lw);
        }
        reply(target, "Filter updated: " + std::to_string(filter.size()) + " words total (" + std::to_string(removed) + " removed)");
    });

    add_command("!listfilter", [&, reply](const std::string &target, const std::string &args){ (void)args;
        if (target.empty() || target[0] != '#') {
            std::string reply_target = (target == currentNick && !last_sender_nick.empty()) ? last_sender_nick : target;
            reply(reply_target, "Filter can only be listed from a channel context");
            return;
        }
        auto fit = channel_filters.find(target);
        if (fit == channel_filters.end() || fit->second.empty()) { reply(target, "Filter empty"); return; }
        std::string list; list.reserve(128);
        for (std::unordered_set<std::string>::const_iterator it = fit->second.begin(); it != fit->second.end(); ++it) {
            if (!list.empty()) list.append(" ");
            list.append(*it);
            if (list.size() > 350) { list.append(" ..."); break; }
        }
        reply(target, "Filter(" + std::to_string(fit->second.size()) + "): " + list);
    });
}

void Bot::handle_message(const Message &msg) {
    log_message(msg);

    if (msg.command == "PING") {
        std::string token = msg.params.empty() ? "" : msg.params[0];
        send_line("PONG :" + token); return;
    }

    // Welcome / end of MOTD
    if (msg.command == "001" || msg.command == "376" || msg.command == "422") {
        if (!autoJoined && !cfg.channels.empty()) { join_channels(); autoJoined = true; }
        return;
    }

    if (msg.command == "433") { // nick in use
        ++nick_attempt; currentNick = baseNick + "_" + std::to_string(nick_attempt);
        std::cerr << "[bot] nick in use, trying " << currentNick << "\n";
        send_line("NICK " + currentNick); return;
    }

    if (msg.command == "353" && msg.params.size() >= 4) { // RPL_NAMREPLY
        const std::string &chan = msg.params[2];
        std::string names = msg.params[3]; if (!names.empty() && names[0] == ':') names.erase(0,1);
        std::istringstream nss(names); std::string token; auto &ops = channel_ops[chan];
        while (nss >> token) if (!token.empty() && token[0] == '@') { std::string opNick = token.substr(1); if (!opNick.empty()) ops.insert(opNick); }
        return;
    }

    if (msg.command == "MODE" && msg.params.size() >= 2) {
        const std::string &chan = msg.params[0]; if (!(chan.size() && chan[0] == '#')) return;
        const std::string &modes = msg.params[1]; std::vector<std::string> modeArgs; for (size_t i=2;i<msg.params.size();++i) modeArgs.push_back(msg.params[i]);
        bool adding = true; size_t argIndex = 0; auto &ops = channel_ops[chan];
        for (char c : modes) {
            if (c == '+') { adding = true; continue; }
            if (c == '-') { adding = false; continue; }
            if (c == 'o' && argIndex < modeArgs.size()) {
                const std::string &targetNick = modeArgs[argIndex++]; if (targetNick.empty()) continue;
                if (adding) ops.insert(targetNick); else ops.erase(targetNick);
            }
        }
        return;
    }

    if (msg.command == "PRIVMSG" && msg.params.size() >= 2) {
        const std::string &target = msg.params[0]; const std::string &text = msg.params[1];
        std::string sender_nick = extract_nick_from_prefix(msg.prefix);

        if (!text.empty() && text[0] == '!') { // command
            size_t spc = text.find(' ');
            std::string key = (spc == std::string::npos) ? text : text.substr(0, spc);
            std::string args = (spc == std::string::npos) ? "" : text.substr(spc + 1);
            auto it = commands.find(key);
            if (it != commands.end()) {
                std::string reply_target = (target == currentNick && !sender_nick.empty()) ? sender_nick : target;
                last_sender_nick = sender_nick; it->second(reply_target, args);
            }
        }
        if (!sender_nick.empty() && sender_nick != currentNick && target.size() && target[0] == '#' && (text.empty() || text[0] != '!')) {
            auto cfIt = channel_filters.find(target);
            if (cfIt != channel_filters.end() && !cfIt->second.empty()) {
                const auto &filter = cfIt->second;
                std::string token; auto flush_token = [&]() {
                    if (token.empty()) return false;
                    if (filter.find(token) != filter.end()) {
                        send_line("KICK " + target + " " + sender_nick + " :filtered word");
                        return true;
                    }
                    token.clear();
                    return false;
                };
                for (char ch : text) {
                    if (std::isalnum(static_cast<unsigned char>(ch))) token.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
                    else if (flush_token()) break;
                }
                if (!token.empty()) flush_token();
            }
        }
        return;
    }

    if (msg.command == "JOIN" && msg.params.size() >= 1) {
        std::string selfNick = extract_nick_from_prefix(msg.prefix);
        if (selfNick == currentNick) { const std::string &chan = msg.params[0]; if (!chan.empty()) joined_channels.insert(chan); }
        return;
    }

    if (msg.command == "QUIT") {
        std::string quitter = extract_nick_from_prefix(msg.prefix);
        for (auto &pair : channel_ops) pair.second.erase(quitter);
        return;
    }

    if (msg.command == "PART" && msg.params.size() >= 1) {
        std::string parter = extract_nick_from_prefix(msg.prefix);
        const std::string &chan = msg.params[0]; auto it = channel_ops.find(chan); if (it != channel_ops.end()) it->second.erase(parter); return;
    }

    if (msg.command == "INVITE" && msg.params.size() >= 2) {
        const std::string &invitee = msg.params[0]; const std::string &channel = msg.params[1];
        if (invitee == currentNick && channel.size()) {
            if (std::find(cfg.channels.begin(), cfg.channels.end(), channel) == cfg.channels.end()) cfg.channels.push_back(channel);
            if (joined_channels.find(channel) == joined_channels.end()) send_line("JOIN " + channel);
        }
        return;
    }
}

void Bot::send_quit(const std::string &reason) {
    if (sockfd < 0) {
        return;
    }
    send_line("QUIT :" + reason);
    ::close(sockfd);
    sockfd = -1;
}

void Bot::run() {
    std::signal(SIGINT, handle_stop); std::signal(SIGTERM, handle_stop); std::signal(SIGPIPE, SIG_IGN);
    baseNick = cfg.nick; currentNick = cfg.nick;

    while (!g_stop) {
        sockfd = connect_socket();
        if (sockfd < 0) {
            ++retries; std::cerr << "[bot] connect failed (" << retries << "/" << MAX_RETRIES << "), retry in " << backoff << "s\n";
            if (retries >= MAX_RETRIES) { std::cerr << "[bot] maximum retries reached, exiting\n"; break; }
            for (int s=0; s<backoff && !g_stop; ++s) sleep(1);
            backoff = std::min(backoff_max, backoff * 2); continue;
        }
        std::cerr << "[bot] connected\n"; retries = 0; backoff = 2; autoJoined = false; nick_attempt = 0;

        login();

        std::queue<std::unique_ptr<Message>> msg_queue; RecvParser parser(msg_queue);

        while (!g_stop) {
            struct pollfd pfd{sockfd, POLLIN, 0}; int pr = ::poll(&pfd, 1, 10000);
            if (pr < 0) { if (errno == EINTR) continue; std::cerr << "poll error: " << std::strerror(errno) << "\n"; break; }
            if (pr == 0) continue;
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) { std::cerr << "[bot] socket closed (treating as server shutdown, exiting)\n"; g_stop = 1; break; }
            if (pfd.revents & POLLIN) {
                char buf[4096]; ssize_t n = ::recv(sockfd, buf, sizeof(buf), 0);
                if (n <= 0) { std::cerr << "[bot] recv <= 0 (treating as server shutdown, exiting)\n"; ::close(sockfd); sockfd = -1; g_stop = 1; break; }
                parser.feed(buf, static_cast<size_t>(n));
                while (!msg_queue.empty()) { auto &ptr = msg_queue.front(); handle_message(*ptr); msg_queue.pop(); }
            }
        }

        if (g_stop && g_userInitiated) { send_quit("Client exiting"); struct pollfd tmp{sockfd, POLLIN, 0}; poll(&tmp, 1, 100); }
        if (sockfd >= 0) { ::close(sockfd); sockfd = -1; }
        if (g_stop) break;

        std::cerr << "[bot] reconnecting in " << backoff << "s\n"; for (int s=0; s<backoff && !g_stop; ++s) sleep(1); backoff = std::min(backoff_max, backoff * 2); cfg.nick = baseNick; currentNick = baseNick; nick_attempt = 0;
    }
}
