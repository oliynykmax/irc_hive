#include "User.hpp"

User::User(Client *cli) : _cli(cli) {

}
unsigned long User::getID() const
{
    return (unsigned long)this;
}

void User::join(Channel *chan) {
	_chan = chan;
}

void User::setNick(string name) {
	_nick = name;
}

void User::setUser(string name) {
	_user = name;
}
