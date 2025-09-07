#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/wait.h>
#include <cerrno>

/*
 * Super minimal IRC bot:
 * - Connects to server: host port nick channel (password optional as 5th arg)
 * - Joins channel
 * - Every PRIVMSG in channel addressed to the bot (contains its nick) => sends it to local AI helper binary (ai_test) via a fork/exec
 *   (Assumes ai_test is in ./bot/ai_test and writes answer to stdout only)
 * - Replies with the AI output truncated to 400 bytes
 *
 * NOTE:
 *  - This is intentionally minimal; no robust IRC parsing, no PING/PONG handling (add if needed).
 *  - For production you must add PING handling or connection will timeout on most servers.
 */

static std::string readAllFromProcess(const std::vector<std::string>& args, const std::string& input = "") {
    int inPipe[2];
    int outPipe[2];
    if (pipe(inPipe) || pipe(outPipe)) return "";

    pid_t pid = fork();
    if (pid == 0) {
        // Child
        dup2(inPipe[0], STDIN_FILENO);
        dup2(outPipe[1], STDOUT_FILENO);
        dup2(outPipe[1], STDERR_FILENO);
        close(inPipe[1]); close(outPipe[0]);
        std::vector<char*> cargs;
        for (auto &a : args) cargs.push_back(const_cast<char*>(a.c_str()));
        cargs.push_back(nullptr);
        execvp(cargs[0], cargs.data());
        _exit(127);
    }
    // Parent
    close(inPipe[0]); close(outPipe[1]);
    if (!input.empty()) {
        const char* data = input.data();
        size_t total = input.size();
        size_t sent = 0;
        while (sent < total) {
            ssize_t w = write(inPipe[1], data + sent, total - sent);
            if (w < 0) {
                if (errno == EINTR)
                    continue;
                break; // give up on unrecoverable error
            }
            if (w == 0) {
                // Should not happen for a pipe unless full + non-blocking (not our case), break to avoid spin
                break;
            }
            sent += static_cast<size_t>(w);
        }
    }
    close(inPipe[1]);

    std::string out;
    char buf[1024];
    ssize_t n;
    while ((n = read(outPipe[0], buf, sizeof(buf))) > 0) {
        out.append(buf, n);
    }
    close(outPipe[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    return out;
}

static bool sendLine(int fd, const std::string& line) {
    std::string wire = line + "\r\n";
    size_t sent = 0;
    while (sent < wire.size()) {
        ssize_t n = send(fd, wire.data() + sent, wire.size() - sent, 0);
        if (n <= 0) return false;
        sent += (size_t)n;
    }
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " <host> <port> <nick> <#channel> [password]" << std::endl;
        return 1;
    }
    std::string host = argv[1];
    std::string port = argv[2];
    std::string nick = argv[3];
    std::string channel = argv[4];
    std::string password = (argc >= 6) ? argv[5] : "";

    // Resolve
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
        if (sock < 0) continue;
        if (::connect(sock, p->ai_addr, p->ai_addrlen) == 0) break;
        close(sock); sock = -1;
    }
    freeaddrinfo(res);
    if (sock < 0) {
        std::cerr << "Connection failed" << std::endl;
        return 1;
    }

    if (!password.empty())
        sendLine(sock, "PASS " + password);
    sendLine(sock, "NICK " + nick);
    sendLine(sock, "USER " + nick + " 0 * :" + nick);
    sendLine(sock, "JOIN " + channel);

    std::string buffer;
    buffer.reserve(8192);

    while (true) {
        char temp[2048];
        ssize_t n = recv(sock, temp, sizeof(temp), 0);
        if (n <= 0) break;
        buffer.append(temp, n);

        // Process complete lines
        size_t pos;
        while ((pos = buffer.find("\r\n")) != std::string::npos) {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 2);

            // Basic PING/PONG
            if (line.rfind("PING", 0) == 0) {
                sendLine(sock, "PONG " + line.substr(5));
                continue;
            }

            // Look for PRIVMSG
            // Format: :prefix PRIVMSG target :message
            size_t priv = line.find(" PRIVMSG ");
            if (priv != std::string::npos) {
                size_t msgStart = line.find(" :", priv);
                if (msgStart != std::string::npos) {
                    std::string target = line.substr(priv + 9, msgStart - (priv + 9));
                    std::string text = line.substr(msgStart + 2);
                    // If it is channel message and contains our nick, call AI
                    bool addressed = (target == channel && text.find(nick) != std::string::npos);
                    // Direct message
                    if (target == nick) addressed = true;

                    if (addressed) {
                        std::string prompt = text;
                        // Strip nick mention
                        if (prompt.find(nick) != std::string::npos) {
                            size_t p;
                            while ((p = prompt.find(nick)) != std::string::npos) {
                                prompt.erase(p, nick.size());
                            }
                        }
                        // Trim spaces
                        while (!prompt.empty() && isspace((unsigned char)prompt.front())) prompt.erase(prompt.begin());
                        while (!prompt.empty() && isspace((unsigned char)prompt.back())) prompt.pop_back();

                        std::vector<std::string> aiArgs = {"./bot/ai_test"};
                        std::string aiOut = readAllFromProcess(aiArgs, prompt + "\n");
                        if (aiOut.size() > 400) aiOut.erase(400);
                        std::stringstream reply;
                        reply << "PRIVMSG " << (target == nick ? line.substr(1, line.find('!') - 1) : channel)
                              << " :" << aiOut;
                        sendLine(sock, reply.str());
                    }
                }
            }
        }
    }

    close(sock);
    return 0;
}
