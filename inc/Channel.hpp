#pragma once
#include "User.hpp"
#include "Server.hpp"
//#include "Operator.hpp"
#include <cstddef>
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
 * @param _passwd password of the channel
 * @param _topic what is discussed on the channel
 * @param _users set containing sockets
 * @param _oper set for the sockets of operators
 * @param _mode set of mode
 */
class Channel {
	private:
		std::string _name;
		std::string _passwd = "";
		std::string _topic;
		size_t _limit;
		set<int> _users;
		set<int> _oper;
		set<int> _invite;
		set<char> _mode;
		bool joinWithPassword(int fd, std::string passwd);
		bool joinWithInvite(int fd, std::string passwd);
		bool checkUser(int fd);
	public:
		explicit Channel(std::string channel);
		bool isEmpty(void) const;
		void setPassword(std::string passwd);
		const std::string& getName(void) const;
		const set<char>& getMode(void) const;
		const set<int>& getUsers(void) const;
		const set<int>& getOperators(void) const;
		const std::string& getTopic(void) const;
		const size_t& getLimit(void) const;
		void setLimit(size_t limit);
		void setMode(std::string mode);
		void unsetMode(std::string umode);
		bool setTopic(int fd, std::string topic);
		const std::string addUser(int fd, std::string passwd = "");
		void removeUser(int fd, std::string msg = "", std::string cmd = "");
		std::string userList(void) const;
		bool makeOperator(int newOp);
		bool kick(int op, int user);
		void invite(int fd);
		bool message(int fd, std::string name = "", std::string msg = "", std::string type = "");
};
