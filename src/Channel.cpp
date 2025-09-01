#include "Channel.hpp"
#include "Operator.hpp"

Channel::Channel(int user, std::string channel) : _name(channel) {
	_oper.emplace(user);
}

bool Channel::addUser(int user) {
	if (_users.contains(user))
		return false;
	_users.emplace(user);
	return true;
}

bool Channel::makeOperator(int user) {
	if (_users.contains(user)) {
		_users.erase(user);
		_oper.emplace(user);
		return true;
	} else {
		return false;
	}
}

bool Channel::kick(int user) {
	if (_users.contains(user)) {
		_users.erase(user);
		return true;
	} else {
		return false;
	}
}

void Channel::message(int user, std::string msg) {
	std::string message = ":" + irc->getClient(fd)->getUser.getNick() + "PRIVMSG" + _name + msg;

	for (users : _users) {
		if (users == user)
			continue;
		if (message.size() == send(users, message.data(), message.size(), 0))
			continue;
		else
			throw std::runtime_error("Channel::message: Error; sending to " + fd + " failed.");
	}

	for (users : _oper) {
		if (users == user)
			continue;
		if (message.size() == send(users, message.data(), message.size(), 0))
			continue;
		else
			throw std::runtime_error("Channel::message: Error; sending to " + fd + " failed.");
	}
}
