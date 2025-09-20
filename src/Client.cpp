#include "Client.hpp"


Client::Client(int fd) : _self(new User()),  _test(new CommandDispatcher()), _fd(fd) {}

Client::Client(const Client& other) :
_IN(other._IN),
_RDHUP(other._RDHUP),
_HUP(other._HUP),
_self(new User(*other._self)),
_test(other._test),
_fd(other._fd),
_initialized(other._initialized){}

Client& Client::operator=(const Client& other) {
	if (this != &other) {
		delete _self;
		_self = new User(*other._self);
		_test = other._test;
	}
	return *this;
}

Client::~Client() = default;

User* Client::getUser(void) {
	return _self;
}

CommandDispatcher* Client::getDispatch(void) {
	return _test;
}

void Client::setHandler(uint32_t eventType, std::function<void(int)> handler) {
	switch (eventType) {
		case EPOLLIN:
			_IN = std::move(handler);
			break;
		case EPOLLRDHUP:
			_RDHUP = std::move(handler);
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
		case EPOLLRDHUP:
			return _RDHUP != nullptr;
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
		case EPOLLRDHUP:
			return _RDHUP;
		case EPOLLHUP:
			return _HUP;
		default:
			throw std::runtime_error("Client::getHandler: Error; invalid eventType");
	}
}

void Client::authenticate(void) {
	_authenticated = true;
}

bool Client::isAuthenticated(void) const {
	return _authenticated;
}

bool& Client::accessRegistered(void){
	return _registered;
}
