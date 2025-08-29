#pragma once
#include "User.hpp"
//#include "Operator.hpp"
#include <vector>

using std::vector;

class User;

/*
 * @class Channel
 * @brief Like rooms that can have Users and 1 or more Operators
 * @param _users vector containing User classes
 */
class Channel {
	private:
		vector<User> _users;
	public:
		void addUser(User user);
		void makeOperator(User user);
		void kick(User user);
};
