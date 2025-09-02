#include "Channel.hpp"
#include "Operator.hpp"

Channel::Channel(std::string channel) : _name(channel) {
}

bool Channel::isEmpty(void) const {
	return _users.empty() && _oper.empty();
}

void Channel::setPassword(std::string passwd) {
	_passwd = passwd;
}

const std::string& Channel::getName(void) const {
	return _name;
}

void Channel::unsetMode(std::string umode)  {
	for (char c : umode) {
		_mode.erase(c);
	}
}

const set<char>& Channel::getMode(void) const {
	return _mode;
}

const set<int>& Channel::getUsers(void) const {
	return _users;
}

const set<int>& Channel::getOperators(void) const {
	return _oper;
}

const std::string& Channel::getTopic(void) const {
	return _topic;
}

const size_t& Channel::getLimit(void) const {
	return _limit;
}

void Channel::setMode(std::string mode) {
	for (char c : mode) {
		_mode.emplace(c);
	}
}

bool Channel::setTopic(int user, std::string topic) {
	if(_oper.contains(user)) {
		_topic = topic;
		std::string message = ":" + irc->getClient(user).getUser()->getNick() + " TOPIC " + _name + " :" + topic + "\r\n";
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
			if (bytes == -1)
				return false;
		}
	} else {
		return false;
	}
	return true;
}

bool Channel::checkUser(int user) {
	if (_users.contains(user) || _oper.contains(user))
		return false;
	return true;
}

bool Channel::addUser(int user) {
	if (checkUser(user)) {
		if (isEmpty()) {
			_oper.emplace(user);
		} else {
			_users.emplace(user);
		}
		irc->getClient(user).getUser()->join(this);
		return true;
	}
	return false;
}

bool Channel::joinWithPassword(int fd, std::string passwd) {
	if (passwd == _passwd) {
		_users.emplace(fd);
		irc->getClient(fd).getUser()->join(this);
		return true;
	} else {
		return false;
	}
}

std::string Channel::userList(void) const {
	std::string ret;

	for (auto users : _users) {
		ret += irc->getClient(users).getUser()->getNick();
		ret += " ";
	}
	for (auto users : _oper) {
		ret += irc->getClient(users).getUser()->getNick();
		ret += " ";
	}
	ret.erase(ret.end() - 1);
	return ret;
}

bool Channel::makeOperator(int op, int newOp) {
	if (_oper.contains(op) && _users.contains(newOp)) {
		_users.erase(newOp);
		_oper.emplace(newOp);
		return true;
	} else {
		return false;
	}
}

bool Channel::kick(int op, int user) {
	if (_users.contains(user) && _oper.contains(op)) {
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
