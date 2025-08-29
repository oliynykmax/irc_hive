#include "Client.hpp"


Client::Client(int fd) :
_self(new User()),
_fd(fd) {
	if (fcntl(fd, F_SETFL, O_NONBLOCK))
		throw std::runtime_error("Client::Client: Fcntl failed");
}

Client::Client(const Client& other) :
_IN(other._IN),
_OUT(other._OUT),
_RDHUP(other._RDHUP),
_PRI(other._PRI),
_ERR(other._ERR),
_HUP(other._HUP),
_self(new User(*other._self)),
_fd(other._fd),
_initialized(other._initialized){}

Client& Client::operator=(const Client& other) {
	if (this != &other) {
		delete _self;
		_self = new User(*other._self);
	}
	return *this;
}

Client::~Client() {
	delete _self;
}

User* Client::getUser(void) {
	return _self;
}

void Client::setHandler(uint32_t eventType, std::function<void(int)> handler) {
	switch (eventType) {
		case EPOLLIN:
			_IN = std::move(handler);
			break;
		case EPOLLOUT:
			_OUT = std::move(handler);
			break;
		case EPOLLRDHUP:
			_RDHUP = std::move(handler);
			break;
		case EPOLLPRI:
			_PRI = std::move(handler);
			break;
		case EPOLLERR:
			_ERR = std::move(handler);
			break;
		case EPOLLHUP:
			_HUP = std::move(handler);
			break;
		default:
			break;
	}
}

bool Client::handler(uint32_t eventType) const {
	switch (eventType) {
		case EPOLLIN:
			return _IN != nullptr;
		case EPOLLOUT:
			return _OUT != nullptr;
		case EPOLLRDHUP:
			return _RDHUP != nullptr;
		case EPOLLPRI:
			return _PRI != nullptr;
		case EPOLLERR:
			return _ERR != nullptr;
		case EPOLLHUP:
			return _HUP != nullptr;
		default:
			return false;
	}
}

std::function<void(int)>& Client::getHandler(uint32_t eventType) {
	switch (eventType) {
		case EPOLLIN:
			return _IN;
		case EPOLLOUT:
			return _OUT;
		case EPOLLRDHUP:
			return _RDHUP;
		case EPOLLPRI:
			return _PRI;
		case EPOLLERR:
			return _ERR;
		case EPOLLHUP:
			return _HUP;
		default:
			throw std::runtime_error("Client::getHandler: Error; invalid eventType");
	}
}
