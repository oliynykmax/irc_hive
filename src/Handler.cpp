#include "Handler.hpp"

using namespace std;

void Handler::clientWrite(int fd) {
	ssize_t messageLen = 0;
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
		if (!dispatcher.dispatch(msg, fd))
			return ;
		msg_queue.pop();
	}
}

void Handler::clientDisconnect(int fd) {
	cout << "Client with socket" << fd << " disconnected" << endl;
}

void Handler::acceptClient(int socket) {
	int fd;
	struct sockaddr_in remote{};
	socklen_t remoteLen{};

	fd = accept4(socket, (struct sockaddr*) &remote, &remoteLen, O_NONBLOCK);

	if (fd > 0) {
		cout << "A client with fd nbr " << fd << " connected" << endl;
		irc->addClient(fd);
		irc->registerHandler(fd, EPOLLIN, clientWrite);
		irc->registerHandler(fd, EPOLLRDHUP | EPOLLHUP, clientDisconnect);
	} else {
		throw runtime_error("Handler::acceptClient: Failed creating a new TCP connection to client");
	}
}
