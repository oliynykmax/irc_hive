#include "User.hpp"

// User::User(string user, string nick)
// :
// _nick(nick),
// _user(user){}

void User::join(Channel *chan) {
	_channels.push_back(chan);
}

void User::quit(int fd, string msg) {
	for (auto channels : _channels) {
		channels->removeUser(fd, msg);
	}
	irc->removeClient(fd);
}

int User::invite(string nick, string name) {
	if (!irc->channelExists(name))
		return -1;
	else if (!irc->nickReserved(nick))
		return 401;
	else if (nick == _nick)
		return -443;
	auto chan = addChannel(name);
	const auto op = chan.getOperators();
	if (!(chan.getUsers().contains(nick) || op.contains(nick)))
		return 443;
	else if (!chan.getMode().contains('i'))
		return -1;
	else if (!op.contains(_nick))
		return 482;
	return 341;
}

void User::setNick(string name) {
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
