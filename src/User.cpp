#include "User.hpp"

// User::User(string user, string nick)
// :
// _nick(nick),
// _user(user){}

void User::join(Channel *chan) {
	_channels.push_back(chan);
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
