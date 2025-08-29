#pragma once
#include "User.hpp"
#include "Channel.hpp"

class User;

enum mode {
	i,
	t,
	k,
	o,
	l
};


class Operator : public User {
	public:
		void kick(string username);
		void invite(string username);
		void topic(Channel *chan);
		void mode(enum mode a, string option);
};
