#pragma once
#include "User.hpp"
#include "Channel.hpp"

class User;


class Operator : public User {
	public:
		void kick(string username);
		void invite(string username);
		void topic(Channel *chan);
		void mode(string option);
};
