#include "Channel.hpp"
#include "Operator.hpp"

// Channel::Channel(Operator admin) {
// 	_operators.emplace(0, admin);
// }

void Channel::addUser(User user) {
	_users.emplace(_users.end(), user);
}

void Channel::makeOperator(User user) {
	(void)user;
}

void Channel::kick(User user) {
	int idx = 0;
	for (User users : _users) {
		if (&user == &users) {
			_users.erase(_users.begin() + idx);
			break;
		}
		idx++;
	}
}
