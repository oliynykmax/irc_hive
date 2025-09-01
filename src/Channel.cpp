#include "Channel.hpp"
#include "Operator.hpp"

Channel::Channel(int user) {
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
