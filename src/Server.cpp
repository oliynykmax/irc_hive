#include "Server.hpp"

Server::Server(std::string passwd) :
_startTime(time(NULL)),
_fd(epoll_create1(0)),
_password(passwd)
{
	if(_fd == -1)
		throw std::runtime_error("Server::Server: ERROR - Failed to create epoll file");
	_events.reserve(_max_events * sizeof(epoll_event));
}

Server::~Server() { close(_fd); }

bool Server::checkPassword(std::string password) const {
	return _password == password;
}

int Server::getServerFd() const {
	return _fd;
}

Channel& Server::addChannel(std::string name) {
	_channels.try_emplace(name, Channel(name));
	return _channels.at(name);
}

void Server::removeChannel(std::string name) {
	_channels.erase(name);
}

bool Server::channelExists(std::string name) {
	return _channels.contains(name);
}

Channel* Server::findChannel(std::string name) {
	auto ret = _channels.find(name);
	if (ret == _channels.end())
		return nullptr;
	return &(ret->second);
}

void Server::addClient(Client& client) {
	_clients.try_emplace(client._fd, client);
}

void Server::removeClient(const int fd) {
	if(!_clients.empty())
		_clients.erase(fd);
	close(fd);
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

		if (client._initialized) {
			epoll_ctl(this->_fd, EPOLL_CTL_MOD, client._fd, &ev);
		} else {
			epoll_ctl(this->_fd, EPOLL_CTL_ADD, client._fd, &ev);
			client._initialized = true;
		}
	}
}

void Server::poll(int tout) {
	int nbrEvents = epoll_wait(_fd, &_events[0], _max_events, tout);

	for (int idx = 0; idx < nbrEvents; idx++) {
		uint32_t event = _events[idx].events;
	 	int fd = _events[idx].data.fd;

		for (uint32_t type : eventTypes) {
			if (_clients.count(fd) == 0)
				return ;
			if (_clients.at(fd).handler(type & event))
				auto fut = std::async(std::launch::async, _clients.at(fd).getHandler(type & event), fd);
		}

		if (event & (EPOLLRDHUP & EPOLLHUP))
			removeClient(fd);
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

std::string Server::getTime(void) const {
	return ctime(&_startTime);
}

const std::unordered_map<int, class Client>& Server::getClients() const {
	return (_clients);
}

Client& Server::getClient(int fd) {
	if (_clients.count(fd) == 0)
		throw std::runtime_error("Server::getClient: Error - no such file descriptor");

	return (_clients.at(fd));
}
