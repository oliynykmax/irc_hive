#include "Server.hpp"
#include "RecvParser.hpp"
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <csignal>

using namespace std;
Server *irc;

void clientWrite(int fd) {
	int messageLen = 0;
	string msg;
	vector<char> buf(BUFSIZ);
	RecvParser	parser;

	messageLen = recv(fd, &buf[0], buf.size(), 0);
	if (messageLen == -1)
		throw runtime_error("Failure receiving message");
	parser.feed(&buf[0], buf.size());
	/*else
		msg.append(buf.cbegin(), buf.cend() + messageLen);
	msg.erase('\n');
	cout << msg << endl;*/
	send(fd, "Message received\n", 18, 0);
}

void clientDisconnect(int fd) {
	cout << "Client with socket" << fd << " disconnected" << endl;
}

void acceptClient(int socket) {
	int fd;
	struct sockaddr_in remote{};
	socklen_t remoteLen{};

	fd = accept(socket, (struct sockaddr*) &remote, &remoteLen);

	if (fd > 0) {
		cout << "A client connected with fd nbr " << fd << " connected" << endl;
		Client client(fd);
		irc->addClient(client);
		irc->registerHandler(fd, EPOLLIN, clientWrite);
		irc->registerHandler(fd, EPOLLRDHUP | EPOLLHUP, clientDisconnect);
	} else {
		throw runtime_error("Failed creating a new TCP connection to client");
	}
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

 	// struct epoll_event event;
  //   vector<epoll_event> events;
  //   events.reserve(sizeof(epoll_event) * 10);

  //   event.events = EPOLLIN;
  //   event.data.fd = sock;
  //   epoll_ctl(irc->getServerFd(), EPOLL_CTL_ADD, sock, &event);
    irc->addOwnSocket(sock);
    irc->registerHandler(sock, EPOLLIN, [](int socket) { acceptClient(socket); });
	while (true) {
		try {
			irc->poll();
		} catch (runtime_error &err) {
			cerr << err.what() << endl;
		}
	}
	close(sock);
	delete irc;
}
