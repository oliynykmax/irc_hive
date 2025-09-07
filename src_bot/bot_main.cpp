/*
 * Simple IRC Bot Client
 *
 * Build (example):
 *   c++ -std=c++20 -Wall -Wextra -Werror -Iinc src_bot/bot_main.cpp -o ircbot
 *
 * Usage:
 *   ./ircbot <host> <port> <nick> [password] [#autojoin]
 *
 * Description:
 *   - Connects to the given IRC server.
 *   - If password provided: sends PASS first.
 *   - Sends NICK / USER.
 *   - Optionally JOINs the provided channel (must start with '#').
 *   - Responds to:
 *       PING                => PONG
 *       PRIVMSG <bot_nick>  => Replies with a canned message
 *       PRIVMSG !echo text  => Echoes text back to the sender/channel
 *       PRIVMSG !quit       => Sends QUIT and exits gracefully
 *
 * Notes:
 *   - Keeps connection alive with automatic PING timeout watchdog (optional).
 *   - Parses only a subset of IRC messages sufficient for basic bot behavior.
 *   - Designed to stay self-contained (no dependency on server internals).
 */

#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <cctype>
#include <chrono>
#include <thread>

#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <poll.h>

// -------------------------------------------------------------
// Utility helpers
// -------------------------------------------------------------

static void fatal(const std::string &msg) {
	std::cerr << "ERROR: " << msg << std::endl;
	std::exit(EXIT_FAILURE);
}

static void debug(const std::string &msg) {
#ifdef BOT_DEBUG
	std::cerr << "[DEBUG] " << msg << std::endl;
#else
	(void)msg;
#endif
}

static void sendLine(int fd, const std::string &line) {
	std::string wire = line + "\r\n";
	ssize_t total = 0;
	while (total < (ssize_t)wire.size()) {
		ssize_t n = ::send(fd, wire.data() + total, wire.size() - total, 0);
		if (n < 0) {
			if (errno == EINTR) continue;
			throw std::runtime_error("send failed: " + std::string(std::strerror(errno)));
		}
		total += n;
	}
	debug(">>> " + line);
}

// -------------------------------------------------------------
// IRC Message representation + minimal parser
// -------------------------------------------------------------

struct IrcMessage {
	std::optional<std::string> prefix;
	std::string command;
	std::vector<std::string> params;
};

static bool parseIrcLine(const std::string &raw, IrcMessage &out) {
	out = IrcMessage();
	if (raw.empty())
		return false;

	std::string line = raw;
	if (line.size() > 512)
		return false;

	std::istringstream iss(line);
	std::string token;

	if (line[0] == ':') {
		if (!(iss >> token))
			return false;
		out.prefix = token.substr(1);
	}
	if (!(iss >> out.command))
		return false;

	while (iss >> token) {
		if (token.size() && token[0] == ':') {
			std::string trailing = token.substr(1);
			std::string rest;
			std::getline(iss, rest);
			if (!rest.empty()) {
				// getline keeps the leading space
				trailing += rest;
			}
			out.params.push_back(trailing);
			break;
		} else {
			out.params.push_back(token);
		}
	}
	return true;
}

// Extract nick from a prefix of the form nick!user@host
static std::string nickFromPrefix(const std::optional<std::string> &prefix) {
	if (!prefix) return "";
	auto &p = *prefix;
	size_t excl = p.find('!');
	if (excl == std::string::npos)
		return p;
	return p.substr(0, excl);
}

// -------------------------------------------------------------
// Bot behavior
// -------------------------------------------------------------

class IrcBot {
public:
	IrcBot(std::string host,
	       std::string port,
	       std::string nick,
	       std::string password,
	       std::optional<std::string> autoJoin)
		: _host(std::move(host)),
		  _port(std::move(port)),
		  _nick(std::move(nick)),
		  _password(std::move(password)),
		  _autoJoin(std::move(autoJoin)) {}

	void run() {
		connectSocket();
		handshake();
		mainLoop();
	}

private:
	std::string _host;
	std::string _port;
	std::string _nick;
	std::string _password;
	std::optional<std::string> _autoJoin;

	int _fd {-1};
	std::string _recvBuffer;
	bool _running {true};
	std::chrono::steady_clock::time_point _lastServerActivity = std::chrono::steady_clock::now();

	void connectSocket() {
		addrinfo hints{};
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		addrinfo *res = nullptr;
		int rc = getaddrinfo(_host.c_str(), _port.c_str(), &hints, &res);
		if (rc != 0)
			fatal("getaddrinfo: " + std::string(gai_strerror(rc)));

		int fd = -1;
		for (addrinfo *p = res; p; p = p->ai_next) {
			fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
			if (fd < 0) continue;
			if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0) {
				_fd = fd;
				break;
			}
			::close(fd);
			fd = -1;
		}
		freeaddrinfo(res);

		if (fd < 0)
			fatal("Unable to connect to " + _host + ":" + _port);

		debug("Connected to server");
	}

	void handshake() {
		if (!_password.empty())
			sendLine(_fd, "PASS " + _password);
		sendLine(_fd, "NICK " + _nick);
		// <username> <hostname> <servername> :<realname>
		// We reuse nick for simplicity
		sendLine(_fd, "USER " + _nick + " 0 * :" + _nick + " bot");
		if (_autoJoin && !_autoJoin->empty()) {
			if (_autoJoin->front() != '#')
				std::cerr << "Warning: Auto-join channel should start with '#'; ignoring.\n";
		}
	}

	void maybeJoin() {
		static bool joined = false;
		if (!joined && _autoJoin && !_autoJoin->empty() && _autoJoin->front() == '#') {
			sendLine(_fd, "JOIN " + *_autoJoin);
			joined = true;
		}
	}

	void mainLoop() {
		while (_running) {
			maybeJoin();
			pollfd pfd { _fd, POLLIN, 0 };
			int timeoutMs = 1000; // 1s poll interval
			int pr = ::poll(&pfd, 1, timeoutMs);
			if (pr < 0) {
				if (errno == EINTR) continue;
				fatal("poll failed: " + std::string(std::strerror(errno)));
			}
			if (pr == 0) {
				checkTimeout();
				continue;
			}
			if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
				std::cerr << "Server closed connection or error.\n";
				break;
			}
			if (pfd.revents & POLLIN) {
				readData();
				processBuffer();
			}
			checkTimeout();
		}
		cleanup();
	}

	void readData() {
		char buf[2048];
		for (;;) {
			ssize_t n = ::recv(_fd, buf, sizeof(buf), 0);
			if (n < 0) {
				if (errno == EWOULDBLOCK || errno == EAGAIN)
					break;
				if (errno == EINTR)
					continue;
				fatal("recv failed: " + std::string(std::strerror(errno)));
			} else if (n == 0) {
				std::cerr << "Remote closed connection\n";
				_running = false;
				break;
			} else {
				_lastServerActivity = std::chrono::steady_clock::now();
				_recvBuffer.append(buf, n);
				if (n < (ssize_t)sizeof(buf))
					break;
			}
		}
	}

	void processBuffer() {
		normalizeNewlines(_recvBuffer);

		for (;;) {
			auto pos = _recvBuffer.find("\r\n");
			if (pos == std::string::npos)
				break;
			std::string line = _recvBuffer.substr(0, pos);
			_recvBuffer.erase(0, pos + 2);
			if (line.empty())
				continue;

			debug("<<< " + line);
			IrcMessage msg;
			if (!parseIrcLine(line, msg))
				continue;
			handleMessage(msg);
		}
	}

	void handleMessage(const IrcMessage &msg) {
		// Respond to PING
		if (msg.command == "PING") {
			if (!msg.params.empty())
				sendLine(_fd, "PONG :" + msg.params.back());
			else
				sendLine(_fd, "PONG :keepalive");
			return;
		}

		// On successful registration numeric 001 we can auto-join
		if (msg.command == "001") {
			maybeJoin();
			return;
		}

		// Handle PRIVMSG
		if (msg.command == "PRIVMSG" && msg.params.size() >= 2) {
			const std::string &target = msg.params[0];
			const std::string &text = msg.params[1];
			std::string sender = nickFromPrefix(msg.prefix);

			bool isChannel = !target.empty() && target[0] == '#';
			std::string replyTarget = isChannel ? target : sender;

			// If message directly mentions our nick
			if (text.find(_nick) != std::string::npos) {
				sendLine(_fd, "PRIVMSG " + replyTarget + " :Hello " + sender + ", I am an automated bot.");
			}

			// Simple command parsing (commands start with '!')
			if (!text.empty() && text[0] == '!') {
				auto space = text.find(' ');
				std::string cmd = (space == std::string::npos) ? text.substr(1) : text.substr(1, space - 1);
				std::string rest = (space == std::string::npos) ? "" : text.substr(space + 1);

				if (cmd == "echo") {
					if (rest.empty())
						sendLine(_fd, "PRIVMSG " + replyTarget + " :Usage: !echo <text>");
					else
						sendLine(_fd, "PRIVMSG " + replyTarget + " :" + rest);
				} else if (cmd == "quit") {
					sendLine(_fd, "PRIVMSG " + replyTarget + " :Goodbye.");
					gracefulQuit("Bot shutting down");
				} else if (cmd == "help") {
					sendLine(_fd, "PRIVMSG " + replyTarget + " :Commands: !echo !quit !help");
				} else {
					sendLine(_fd, "PRIVMSG " + replyTarget + " :Unknown command. Try !help");
				}
			}
		}
	}

	void gracefulQuit(const std::string &reason) {
		try {
			sendLine(_fd, "QUIT :" + reason);
		} catch (...) {
			// ignore
		}
		_running = false;
	}

	void cleanup() {
		if (_fd >= 0) {
			::close(_fd);
			_fd = -1;
		}
	}

	void checkTimeout() {
		// If no server activity for 5 minutes, send a PING
		auto now = std::chrono::steady_clock::now();
		auto idle = std::chrono::duration_cast<std::chrono::seconds>(now - _lastServerActivity).count();
		if (idle > 300) { // 5 minutes
			try {
				sendLine(_fd, "PING :keepalive");
			} catch (...) {
				_running = false;
			}
			_lastServerActivity = now;
		}
	}

	static void normalizeNewlines(std::string &buf) {
		// Convert lone \n or \r into \r\n for uniform parsing
		std::string norm;
		norm.reserve(buf.size());
		for (size_t i = 0; i < buf.size(); ++i) {
			if (buf[i] == '\r') {
				if (i + 1 < buf.size() && buf[i + 1] == '\n') {
					norm += "\r\n";
					++i;
				} else {
					norm += "\r\n";
				}
			} else if (buf[i] == '\n') {
				norm += "\r\n";
			} else {
				norm += buf[i];
			}
		}
		buf.swap(norm);
	}
};

// -------------------------------------------------------------
// Main
// -------------------------------------------------------------

int main(int argc, char **argv) {
	if (argc < 4 || argc > 6) {
		std::cerr << "Usage: " << argv[0] << " <host> <port> <nick> [password] [#autojoin]\n";
		return 1;
	}

	std::string host = argv[1];
	std::string port = argv[2];
	std::string nick = argv[3];
	std::string password;
	std::optional<std::string> autoJoin;

	if (argc >= 5)
		password = argv[4];
	if (argc == 6)
		autoJoin = argv[5];

	try {
		IrcBot bot(host, port, nick, password, autoJoin);
		bot.run();
	} catch (const std::exception &e) {
		fatal(std::string("Unhandled exception: ") + e.what());
	} catch (...) {
		fatal("Unknown exception");
	}
	return 0;
}