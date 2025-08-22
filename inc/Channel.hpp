#pragma once
#include "User.hpp"
#include "Operator.hpp"
#include <vector>
using std::vector;

class Channel {
	private:
		vector<User> _users;
		vector<Operator> _operators;
	public:
		explicit Channel(User user);
		void addUser(User user);
		void makeOperator(User user);
		void kick(User user);
};
