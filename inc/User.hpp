#pragma once

#include "Client.hpp"
#include "Channel.hpp"
#include <string>

using std::string;

class User {
	private:
		Client *_cli;
		Channel *_chan = nullptr;
		string _nick = "";
		string _user = "";
	public:
		explicit User(Client *cli);
		void join(Channel *chan);
		void setNick(string name);
		void setUser(string name);
  		unsigned long getID() const;
};
