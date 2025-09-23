#include "Client.hpp"

Client::Client(int fd) : _self(User()),  _dispatch(new CommandDispatcher()), _fd(fd) {}

Client::~Client() {
	delete _dispatch;
}

User& Client::getUser(void) {
	return _self;
}

CommandDispatcher* Client::getDispatch(void) {
	return _dispatch;
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
