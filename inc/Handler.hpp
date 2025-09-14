#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include "RecvParser.hpp"
#include "CommandDispatcher.hpp"
#include "Client.hpp"
#include "Server.hpp"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

class Server;
extern Server *irc;

class Handler {
	private:
		Handler() = delete;
	public:
		static void clientWrite(int fd);
		static void clientDisconnect(int fd);
		static void acceptClient(int fd);

};
