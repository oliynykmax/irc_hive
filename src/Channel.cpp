#include "Channel.hpp"
#include "Operator.hpp"

Channel::Channel(Operator op) {
	_operators.emplace(0, op);
}

void Channel::addUser(User user) {
	_users.emplace(_users.end(), user);
}

void Channel::makeOperator(User user) {
	(void)user;
}

void Channel::kick(User user) {
	int idx = 0;
	for (User users : _users) {
		if (user.getID() == users.getID()) {
			_users.erase(idx);
			break;
		}
		idx++;
	}
}
