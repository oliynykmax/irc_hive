#include "Server.hpp"
#include <cstdint>
#include <stdexcept>


Server::Server(std::string passwd) :  _fd(epoll_create1(0)), _password(passwd) {
	if(_fd == -1)
		throw std::runtime_error("Server::Server: ERROR - Failed to create epoll file");

	_events.reserve(_max_events * sizeof(epoll_event));
}

Server::~Server() { close(_fd); }

int Server::getServerFd() const {
	return _fd;
}

void Server::addClient(const Client client) {
	_clients.try_emplace(client._fd, client);
}

void Server::removeClient(const Client client) {
	if(!_clients.empty())
		_clients.erase(client._fd);
}

void Server::_reloadHandler(Client &client) const {
	uint32_t event = 0;
	struct epoll_event ev{};
	ev.data.fd = client._fd;
	bool firstEvent = true;
	for (uint32_t evt: eventTypes) {
		if (client.handler(evt)) {
			event = evt;
			firstEvent = false;
		} else {
			event |= evt;
		}

		if (firstEvent)
			event = EPOLLET;
		else
			event |= EPOLLET;

		ev.events = event;

		if (client.isInitialized) {
			epoll_ctl(this->_fd, EPOLL_CTL_MOD, client._fd, &ev);
		} else {
			epoll_ctl(this->_fd, EPOLL_CTL_ADD, client._fd, &ev);
			client.isInitialized = true;
		}
	}
}

void Server::poll(int tout) {
	int nbrEvents = epoll_wait(_fd, &_events[0], _max_events, tout);
	(void)nbrEvents;

	for (int idx = 0; idx < _max_events; idx++) {
		uint32_t events = _events[idx].events;
	 	int fd = _events[idx].data.fd;
		for (uint32_t eventType : eventTypes) {
			if (_clients.count(fd) == 0)
				return ;
			if (_clients.at(fd).handler(eventType))
				_clients.at(fd).getHandler(eventType)(fd);
		}

	if (events & (EPOLLRDHUP & EPOLLHUP))
		removeClient(_clients.at(fd));
	}
}

void Server::registerHandler(const int fd, uint32_t eventType, std::function<void(int)> handler) {
	if (_clients.count(fd) == 0)
		throw std::runtime_error("Server::registerHandler: Error - no such file descriptor");


	Client &cli = _clients.at(fd);

	for (uint32_t eventT: eventTypes) {
		if (eventT & eventType) {
			cli.setHandler(eventType, handler);
		}
	}

	_reloadHandler(cli);
}

void Server::addOwnSocket(int sockfd) {
	_clients.try_emplace(sockfd, Client(sockfd));
}
