#include "Client.hpp"
#include <stdexcept>
#include <sys/epoll.h>
#include <fcntl.h>

Client::Client(int fd) : _fd(fd) {
	if (fcntl(fd, F_SETFL, O_NONBLOCK))
		throw std::runtime_error("Client::Client: Fcntl failed");
}

void Client::setHandler(uint32_t eventType, std::function<void(int)> handler) {
	switch (eventType) {
		case EPOLLIN:
			IN = std::move(handler);
			break;
		case EPOLLOUT:
			OUT = std::move(handler);
			break;
		case EPOLLRDHUP:
			RDHUP = std::move(handler);
			break;
		case EPOLLPRI:
			PRI = std::move(handler);
			break;
		case EPOLLERR:
			ERR = std::move(handler);
			break;
		case EPOLLHUP:
			HUP = std::move(handler);
			break;
		default:
			break;
	}
}

bool Client::handler(uint32_t eventType) const {
	switch (eventType) {
		case EPOLLIN:
			return IN != nullptr;
		case EPOLLOUT:
			return OUT != nullptr;
		case EPOLLRDHUP:
			return RDHUP != nullptr;
		case EPOLLPRI:
			return PRI != nullptr;
		case EPOLLERR:
			return ERR != nullptr;
		case EPOLLHUP:
			return HUP != nullptr;
		default:
			return false;
	}
}

std::function<void(int)>& Client::getHandler(uint32_t eventType) {
	switch (eventType) {
		case EPOLLIN:
			return IN;
		case EPOLLOUT:
			return OUT;
		case EPOLLRDHUP:
			return RDHUP;
		case EPOLLPRI:
			return PRI;
		case EPOLLERR:
			return ERR;
		case EPOLLHUP:
			return HUP;
		default:
			throw std::runtime_error("Client::getHandler: Error; invalid eventType");
	}
}
