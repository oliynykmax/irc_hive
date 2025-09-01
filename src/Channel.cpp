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

bool Channel::message(int user, std::string msg) {
	std::string message = ":" + irc->getClient(user).getUser()->getNick() + " PRIVMSG " + _name + " :" + msg + "\r\n";

	for (auto users : _users) {
		if (users == user)
			continue;
		auto bytes = send(users, message.data(), message.size(), 0);
		if (bytes == -1)
			return false;
	}

	for (auto users : _oper) {
		if (users == user)
			continue;
		auto bytes = send(users, message.data(), message.size(), 0);
		if (-1 == bytes)
			return false;
	}

	return true;
}
