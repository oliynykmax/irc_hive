#include "User.hpp"

void User::join(Channel *chan) {
	_channels.push_back(chan);
}

void User::quit(int fd, string msg) {
	for (auto channels : _channels) {
		channels->removeUser(fd, msg, "QUIT");
	}
	irc->removeClient(fd);
}

void User::setNick(int fd, string name) {
	for (auto channels : _channels) {
		channels->message(fd, createPrefix(), "NICK", name);
	}
	_nick = name;
}

void User::setUser(string name) {
	_user = name;
}

void User::setHost(string host) {
	_hostname = host;
}

string User::getNick(void) const {
	return _nick;
}

string User::getUser(void) const {
	return _user;
}

string User::getHost(void) const {
	return _hostname;
}

string User::createPrefix(void) const {
	return (":" + _nick + "!" + _user + "@" + _hostname);
}

Channel* User::getChannel(string needle) {
	for (auto channel : _channels) {
		if (channel->getName() == needle)
			return channel;
	}
	return nullptr;
}

void User::exitChannel(string needle) {
	for (int idx = 0; _channels[idx]; idx++) {
		if (_channels[idx]->getName() == needle) {
			_channels.erase(_channels.begin() + idx);
			break;
		}
	}
}
