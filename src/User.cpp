#include "User.hpp"

// User::User(string user, string nick)
// :
// _nick(nick),
// _user(user){}

void User::join(Channel *chan) {
	_channels.push_back(chan);
}

void User::quit(int fd, std::string msg) {
	for (auto channels : _channels) {
		channels->removeUser(fd, msg, "QUIT");
	}
	irc->removeClient(fd);
}

void User::setNick(int fd, string name) {
	for (auto channels : _channels) {
		channels->message(fd, _nick, "NICK", name);
	}
	_nick = name;
}

void User::setUser(string name) {
	_user = name;
}

string User::getNick(void) const {
	return _nick;
}

string User::getUser(void) const {
	return _user;
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
