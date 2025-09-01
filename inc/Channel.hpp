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
 * @param _name what the #channel is called
 * @param _topic what is discussed on the channel
 * @param _users set containing sockets
 * @param _oper set containing sockets of operators
 */
class Channel {
	private:
		std::string _name;
		std::string _topic;
		set<int> _users;
		set<int> _oper;
	public:
		explicit Channel(std::string channel);
		bool setTopic(int fd, std::string topic);
		bool checkUser(int fd);
		bool addUser(int fd);
		bool makeOperator(int op, int newOp);
		bool kick(int op, int user);
		bool message(int fd, std::string msg);
};
