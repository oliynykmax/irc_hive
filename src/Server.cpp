#include "Server.hpp"

Server::Server(std::string port, std::string passwd) :
_startTime(time(nullptr)),
_fd(epoll_create1(0)),
_sock(socket(AF_INET, SOCK_STREAM, 0)),
_checker(0),
_port(stoi(port, &_checker)),
_password(passwd)
{
	if(_fd == -1)
		throw std::runtime_error("Server::Server: ERROR - Failed to create epoll file");
	else if (port[_checker])
		throw std::runtime_error("Server::Server: ERROR - Bad port number " + port);
	auto optval = 1;
	setsockopt(_sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	struct sockaddr_in sa_bindy{};
	sa_bindy.sin_family = AF_INET;
	sa_bindy.sin_port = htons(_port);
	sa_bindy.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(_sock, (struct sockaddr *)&sa_bindy, sizeof(sa_bindy))) {
		close(_fd);
		close(_sock);
		throw std::runtime_error("Server::Server: ERROR - Binding failed to " + port);
	}

	if (listen(_sock, 10)) {
		close(_fd);
		close(_sock);
		throw std::runtime_error("Server::Server: ERROR - Failed listen on port " + port);
	}

	struct epoll_event ev{};
	ev.data.fd = _sock;
	ev.events = EPOLLIN;
	epoll_ctl(this->_fd, EPOLL_CTL_ADD, _sock, &ev);

	_events.reserve(_max_events * sizeof(epoll_event));
}

Server::~Server() {
	for(auto client : _clients) {
		delete client.second.getDispatch();
	}
	close(_fd);
 	close(_sock);
}

bool Server::checkPassword(std::string password) const {
	return _password == password;
}

int Server::getServerFd() const {
	return _fd;
}

Channel& Server::addChannel(std::string name) {
	_channels.try_emplace(name, name);
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

void Server::addClient(int fd) {
	_clients.try_emplace(fd, fd);
}

void Server::removeClient(const int fd) {
	if(not _clients.empty()) {
		delete getClient(fd).getDispatch();
		_clients.erase(fd);
	}
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
		if (fd == _sock) {
			Handler::acceptClient(_sock);
			continue;
		}
		for (uint32_t type : eventTypes) {
			if (_clients.count(fd) == 0)
				return ;
			if (_clients.at(fd).handler(type & event)) {
				[[maybe_unused]]
				auto fut = std::async(std::launch::async, _clients.at(fd).getHandler(type & event), fd);
			}
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

std::string Server::getTime(void) const {
	string ret = ctime(&_startTime);
	ret.pop_back();
	return ret;
}

const std::unordered_map<int, class Client>& Server::getClients() const {
	return (_clients);
}

Client& Server::getClient(int fd) {
	if (_clients.count(fd) == 0)
		throw std::runtime_error("Server::getClient: Error - no such file descriptor");

	return (_clients.at(fd));
}
