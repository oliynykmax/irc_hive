#pragma once
#include "User.hpp"
//#include "Operator.hpp"
#include <set>

using std::set;

class User;

/*
 * @class Channel
 * @brief Like rooms that can have Users and 1 or more Operators
 * @param _users vector containing User classes
 */
class Channel {
	private:
		set<int> _users;
		set<int> _oper;
	public:
		explicit Channel(int fd);
		bool addUser(int fd);
		bool makeOperator(int fd);
		bool kick(int fd);
};
