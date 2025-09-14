#pragma once
#include "User.hpp"
#include "Server.hpp"
#include <set>
#include <ctime>
#include <cstddef>
#include <sys/types.h>
#include <sys/socket.h>

using std::set;
using std::string;

class Server;
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
		time_t _startTime;
		string _name, _passwd, _topic;
		size_t _limit;
		set<int> _users, _oper, _invite;
		set<char> _mode{'s'};
		bool joinWithPassword(int fd, string passwd);
		bool joinWithInvite(int fd, string passwd);
		bool checkUser(int fd);
	public:
		explicit Channel(string channel);
		bool isEmpty(void) const;
		void setPassword(string passwd);
		const string& getName(void) const;
		const set<char>& getMode(void) const;
		const set<int>& getUsers(void) const;
		const set<int>& getOperators(void) const;
		const string& getTopic(void) const;
		const size_t& getLimit(void) const;
		const string getTime(void) const;
		bool setLimit(string limit);
		void setMode(string mode);
		void unsetMode(string umode);
		bool setTopic(int fd, string topic);
		const string addUser(int fd, string passwd = "");
		void removeUser(int fd, string msg = "", string cmd = "");
		string userList(void) const;
		string modes(void) const;
		bool makeOperator(int fd, string user);
		bool kick(int op, int user);
		void invite(int fd);
		bool message(int fd, string name = "", string msg = "", string type = "");
};
