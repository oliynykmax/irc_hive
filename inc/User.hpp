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
		string _hostname = "";
	public:
		void join(Channel *chan);
		void quit(int fd, string msg);
		void setNick(int filde, string name);
		void setUser(string name);
		void setHost(string host);
		string getNick(void) const;
		string getUser(void) const;
		string getHost(void) const;
		string createPrefix(void) const;
		Channel* getChannel(string needle);
		void exitChannel(string needle);
};
