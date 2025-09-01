#pragma once
#include "User.hpp"
#include "Server.hpp"
//#include "Operator.hpp"
#include <set>
#include <sys/types.h>
#include <sys/socket.h>

class Server;

using std::set;

extern Server *irc;

class User;

/*
 * @class Channel
 * @brief Like rooms that can have Users and 1 or more Operators
 * @param _users vector containing User classes
 */
class Channel {
	private:
		std::string _name;
		set<int> _users;
		set<int> _oper;
	public:
		explicit Channel(int fd, std::string channel);
		bool addUser(int fd);
		bool makeOperator(int fd);
		bool kick(int fd);
		void message(int fd, std::string msg);
};
