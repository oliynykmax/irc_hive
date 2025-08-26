#include "Handler.hpp"

using namespace std;

void Handler::clientWrite(int fd) {
	int messageLen = 0;
	string msg;
	vector<char> buf(BUFSIZ);
	queue<unique_ptr<Message>> msg_queue;
	RecvParser	parser(msg_queue);
	CommandDispatcher	dispatcher;

	messageLen = recv(fd, &buf[0], buf.size(), 0);
	if (messageLen == -1)
		throw runtime_error("Failure receiving message");
	parser.feed(&buf[0], messageLen);
	while (!msg_queue.empty())
	{
		const unique_ptr<Message> &msg = msg_queue.front();
		dispatcher.dispatch(msg, fd);
		msg_queue.pop();
	}
	send(fd, "Message received\n", 18, 0);
}

void Handler::clientDisconnect(int fd) {
	cout << "Client with socket" << fd << " disconnected" << endl;
}

void Handler::acceptClient(int socket) {
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
