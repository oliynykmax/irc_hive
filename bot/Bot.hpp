#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <optional>
#include <queue>
struct Message;

class Bot {
public:
    struct Config {
        std::string host = "localhost";
        std::string port = "6667";
        std::string nick = "ircbot";
        std::string pass;
        std::vector<std::string> channels;
    };

    explicit Bot(const Config &cfg);
    ~Bot();

    void run();

    void add_command(const std::string &key,
                     std::function<void(const std::string &, const std::string &)> fn);

private:
    int sockfd;
    Config cfg;
    std::string currentNick;
    std::string baseNick;

    std::unordered_set<std::string> joined_channels;
    std::unordered_map<std::string, std::unordered_set<std::string>> channel_filters; // channel -> banned words
    std::unordered_map<std::string, std::unordered_set<std::string>> channel_ops; // channel -> ops
    std::string last_sender_nick;
    bool autoJoined; // joined channels after welcome numeric

    int nick_attempt;
    int backoff;
    int backoff_max;
    int retries;
    const int MAX_RETRIES = 5;

    // Commands
    std::unordered_map<std::string, std::function<void(const std::string &, const std::string &)>> commands;

    int  connect_socket();
    void login();
    void send_line(const std::string &s);
    void send_privmsg(const std::string &target, const std::string &text);
    void join_channels();

    std::string extract_nick_from_prefix(const std::optional<std::string> &prefix);
    void log_message(const Message &msg);
    void handle_message(const Message &msg);
    void send_quit(const std::string &reason);

    void setup_builtin_commands();
};
