#include "Server.hpp"
#include "Handler.hpp"
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <csignal>

using namespace std;
Server *irc;

namespace {
	volatile std::sig_atomic_t gSigStatus = 0;
}

int main(int argc, char *argv[]) {
	struct sockaddr_in sa_bindy{};
	sa_bindy.sin_family = AF_INET;

	if (argc != 2 && argc != 3) {
		cerr << "Usage ./ircserv [port] <password>" << endl;
		return 1;
	} else if (argc == 3) {
		irc = new Server(argv[2]);
	} else {
		irc = new Server;
	}

	int port;

	try {
		port = stoi(argv[1]);
	} catch (...) {
		cerr << "Faulty port" << endl;
		return 1;
	}

	sa_bindy.sin_port = htons(port);
	sa_bindy.sin_addr.s_addr = htonl(INADDR_ANY);

	auto sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		delete irc;
		cerr << "Socket creation failed" << endl;
		return 1;
	}

	int optval = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

	if (bind(sock, (struct sockaddr *)&sa_bindy, sizeof(sa_bindy))) {
		delete irc;
		close(sock);
		cerr << "Binding failed" << endl;
		return 1;
	}

	if (listen(sock, 10)) {
		delete irc;
		close(sock);
		cerr << "Listening on the port failed" << endl;
		return 1;
	}

	signal(SIGINT, [](int) { gSigStatus = 1; });
	signal(SIGQUIT, [](int) { gSigStatus = 1; });

    irc->addOwnSocket(sock);
    irc->registerHandler(sock, EPOLLIN, [](int socket) { Handler::acceptClient(socket); });

	while (!gSigStatus) {
		try {
			irc->poll();
		} catch (runtime_error &err) {
			cerr << err.what() << endl;
		}
	}
	close(sock);
	delete irc;
}
