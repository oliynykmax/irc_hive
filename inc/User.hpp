#pragma once

#include "Channel.hpp"
#include <string>
#include <vector>

using std::string;
using std::vector;

class Channel;

class User {
	private:
		vector<Channel*> _channels;
		string _nick = "";
		string _user = "";
	public:
		void join(Channel *chan);
		void quit(int fd, std::string msg);
		void setNick(int fd, string name);
		void setUser(string name);
		string getNick(void) const;
		string getUser(void) const;
		Channel* getChannel(string needle);
		void exitChannel(string needle);
};
