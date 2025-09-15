#include "Channel.hpp"

Channel::Channel(std::string channel) : _startTime(time(NULL)), _name(channel), _passwd(), _topic() {
}

bool Channel::isEmpty(void) const {
	return _users.empty() && _oper.empty();
}

void Channel::setPassword(string passwd) {
	_passwd = passwd;
}

const string& Channel::getName(void) const {
	return _name;
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

const string& Channel::getTopic(void) const {
	return _topic;
}

const size_t& Channel::getLimit(void) const {
	return _limit;
}

const string Channel::getTime(void) const {
	return std::to_string(_startTime);
}

bool Channel::setLimit(string limit) {
	try {
		_limit = std::stoul(limit);
		return true;
	} catch (...) {
		return false;
	}
}

void Channel::setMode(string mode) {
	for (char c : mode) {
		if (c == 'o')
			continue;
		_mode.emplace(c);
	}
}

void Channel::unsetMode(string umode)  {
	for (char c : umode) {
		if (c == 'i')
			_invite.clear();
		_mode.erase(c);
	}
}

bool Channel::setTopic(int user, string topic) {
	if(_mode.contains('t') && not _oper.contains(user)) {
		return false;
	}
	_topic = topic;
	string response = ":" + USER(user)->getNick() + " TOPIC " + _name + " :" + topic;
	message(-1, response);
	return true;
}

bool Channel::checkUser(int user) {
	if (_users.contains(user) || _oper.contains(user))
		return false;
	return true;
}

const string Channel::addUser(int user, string passwd) {
	string ret;
	const string nick = USER(user)->getNick();
	if (not checkUser(user))
		ret = E443;
	else if (_mode.contains('l') && _users.size() + _oper.size() >= _limit)
		ret = E471;
	else if (_mode.contains('i'))
		if (_mode.contains('k') && joinWithInvite(user, passwd))
			;
		else if (_mode.contains('k'))
			ret = E475;
		else
			ret = E473;
	else if (_mode.contains('k'))
		if (joinWithPassword(user, passwd))
			;
		else
			ret = E475;
	else if (isEmpty()) {
		_oper.emplace(user);
		USER(user)->join(this);
	} else {
		_users.emplace(user);
		USER(user)->join(this);
	}
	return ret;
}

void Channel::removeUser(int fd, string msg, string cmd) {
	if (_users.contains(fd)) {
		_users.erase(fd);
		message(fd, msg, cmd);
	} else if (_oper.contains(fd)) {
		_oper.erase(fd);
		if  (_oper.empty()) {
			if (_users.empty()) {
					USER(fd)->exitChannel(_name);
					irc->removeChannel(_name);
					return ;
			} else {
				auto user = _users.begin();
				_oper.emplace(*user);
				_users.erase(*user);
				message(fd, msg, cmd);
			}
		} else {
			message(fd, msg, cmd);
		}
	} else {
		return ;
	}
	USER(fd)->exitChannel(_name);
}

bool Channel::joinWithPassword(int fd, string passwd) {
	if (passwd == _passwd) {
		if (isEmpty())
			_oper.emplace(fd);
		else
			_users.emplace(fd);
		USER(fd)->join(this);
		return true;
	} else {
		return false;
	}
}

bool Channel::joinWithInvite(int fd, string passwd) {
	if (_invite.contains(fd)) {
		if (!_mode.contains('k')) {
			if (isEmpty()) {
				_invite.erase(fd);
				_oper.emplace(fd);
			} else {
				_invite.erase(fd);
				_users.emplace(fd);
			}
			irc->getClient(fd).getUser()->join(this);
			return true;
		} else {
			if (joinWithPassword(fd, passwd))
				return true;
			else
				return false;
		}
	} else {
		return false;
	}
}

string Channel::userList(void) const {
	string ret;

	for (auto users : _users) {
		ret += USER(users)->getNick();
		ret += " ";
	}
	for (auto users : _oper) {
		ret += '@';
		ret += USER(users)->getNick();
		ret += " ";
	}
	ret.erase(ret.end() - 1);
	return ret;
}

string Channel::modes(void) const {
	string ret("+");

	for (auto mode : _mode)
		ret += mode;
	if (ret.size() == 1)
		ret.clear();

	return ret;
}

bool Channel::makeOperator(int fd, string uname) {
	int newOp = 0;

	for (auto user : _users) {
		if (USER(user)->getNick() == uname) {
			newOp = user;
			break ;
		}
	}
	if (!newOp)
		return false;
	_users.erase(newOp);
	_oper.emplace(newOp);
	return message(-1, PREFIX + " MODE " + _name + " +o " + uname);
}

void Channel::invite(int fd) {
	_invite.emplace(fd);
}

bool Channel::kick(int op, int user) {
	if (_users.contains(user) && _oper.contains(op)) {
		_users.erase(user);
		USER(user)->exitChannel(_name);
		return true;
	} else {
		return false;
	}
}

bool Channel::message(int user, string msg, string type, string name) {
	string message;

	if (type.empty())
		message = msg + "\r\n";
	else if (name.empty())
		message = ":" + USER(user)->getNick() + " " + type + " " + _name + " :" + msg + "\r\n";
	else
		message = msg + " " + type + " :" + name + "\r\n";

	for (auto users : _users) {
		if (users == user)
			continue;
		else if (-1 == send(users, message.data(), message.size(), 0))
			return false;
	}
	for (auto users : _oper) {
		if (users == user)
			continue;
		if (-1 == send(users, message.data(), message.size(), 0))
			return false;
	}
	return true;
}
