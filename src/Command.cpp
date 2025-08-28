#include "Command.hpp"

static void debugLog(const Message &msg)
{
	std::cout << "[DEBUG] Executed " << msg.command << " command with ";
	if (msg.prefix)
		std::cout << "prefix: " << *msg.prefix << ", ";
	std::cout << "params:";
	for (auto param : msg.params)
		std::cout << " " << param;
	std::cout << std::endl;
}

#include <sys/socket.h>
static void	sendResponse(std::string message, int fd)
{
	message.append("\r\n");
	send(fd, message.c_str(), message.size(), 0);
}

void NickCommand::execute(const Message &msg, int fd)
{
	if (msg.params.empty())
	{
		sendResponse("431 :No nickname given", fd);
		return ;
	}

	std::string	newNick = msg.params[0];

	if (newNick.size() < 1 || newNick.size() > 9)
	{
		sendResponse("432 :Erroneous nickname", fd);
		return ;
	}

	std::regex nickname_regex("^[A-Za-z][A-Za-z0-9-_]*");
	if (!std::regex_match(newNick, nickname_regex))
	{
		sendResponse("432 :Erroneous nickname", fd);
		return ;
	}
	for (auto client : irc->getClients())
	{
		std::cout << "[DEBUG] Checking nick collission with client: " << client.first << std::endl;
	}
	// handle collission
	//		sendResponse("433 :Nickname is already in use", fd);
	// if changing from a previous nickname, broadcast change to others
	std::cout << "[DEBUG] Set client nickname to: " << newNick << std::endl;
}

void UserCommand::execute(const Message &msg, int fd)
{
	if (msg.params.size() < 4)
	{
		sendResponse("461 :Missing parameters", fd);
		return ;
	}
	std::cout << "[DEBUG] Registered username " << msg.params[0];
	std::cout << " and realname " << msg.params[3];
	std::cout << " for client " << fd << std::endl;
	std::cout << "[DEBUG] ignored hostname " << msg.params[1];
	std::cout << " and servername " << msg.params[2] << std::endl;
	// if registered
	//	sendResponse("462 :Already registered", fd);
	//	return ;
	
	// register
	// if registration successfull ->
	sendResponse("001 yournick :Welcome to IRC network", fd);
	sendResponse("002 yournick :Your host is " + msg.params[1], fd);
	sendResponse("003 yournick :This server was created moments ago", fd);
	sendResponse("004 yournick :Your info is " + msg.params[0] + " " + msg.params[3], fd);
}


void JoinCommand::execute(const Message &msg, int fd)
{
	(void)fd;
	debugLog(msg);
}

void PartCommand::execute(const Message &msg, int fd)
{
	(void)fd;
	debugLog(msg);
}

void PrivmsgCommand::execute(const Message &msg, int fd)
{
	if (msg.params.empty())
	{
		sendResponse("411 :No recipient given", fd);
		return ;
	}
	if (msg.params.size() < 2)
	{
		sendResponse("412 :No text to send", fd);
		return ;
	}
	// if client not registered:
	//  sendResponse("451 :You have not registered", fd);
	if (msg.params[0][0] == '#')
	{
		std::cout << "[DEBUG] Sending message \"" << msg.params[1] << "\" ";
		std::cout << "to channel " << msg.params[0] << " if found" << std::endl;
	}
	else
	{
		std::cout << "[DEBUG] Sending message \"" << msg.params[1] << "\" ";
		std::cout << "to user " << msg.params[0] << " if found" << std::endl;
	}
	// if target not found
	//  sendResponse("401 :No such nick/channel", fd);
}

void KickCommand::execute(const Message &msg, int fd)
{
	(void)fd;
	debugLog(msg);
}

void InviteCommand::execute(const Message &msg, int fd)
{
	(void)fd;
	debugLog(msg);
}

void TopicCommand::execute(const Message &msg, int fd)
{
	(void)fd;
	debugLog(msg);
}

void ModeCommand::execute(const Message &msg, int fd)
{
	auto channel = [&]()
	{
		// if user not operator on channel;
		// sendResponse("482 :Channel operator privileges required", fd);
		if (msg.params.size() < 2)
		{
			sendResponse("461 :Need more parameters", fd);
			return ;
		}
		
		if (msg.params[1].size() != 2
				&& msg.params[1][0] != '-' && msg.params[1][0] != '+')
		{
			sendResponse("472 :Unknown mode", fd);
			return ;
		}
		std::string supported = "itkol";
		if (supported.find(msg.params[1][1]) == std::string::npos)
		{
			sendResponse("472 :Unknown mode", fd);
			return ;
		}
		std::string requireParam = "kl";
		if (requireParam.find(msg.params[1][1]) != std::string::npos
				&& msg.params[1][0] == '+'
				&& msg.params.size() < 3)
		{
			sendResponse("461 :Need more parameters", fd);
			return ;
		}
		std::cout << "[DEBUG] Set channel modes " << msg.params[1] << " for " << msg.params[0] << std::endl;
	};

	auto user = [&]()
	{
		// if msg.params[0] != calling user
		// sendResponse("502 :Users don't match", fd);
		if (msg.params.size() < 2)
		{
			std::cout << "[DEBUG] Sending back user modes for: " << msg.params[0] << std::endl;
			return ;
		}
		if (msg.params[1] == "+i")
		{
			std::cout << "[DEBUG] Set user " << msg.params[0] << " invisible" << std::endl;
			return ;
		}
		sendResponse("472 :Unknown mode", fd);
	};

	if (msg.params.size() < 1)
	{
		sendResponse("461 :Need more parameters", fd);
		return ;
	}
	if (msg.params[0][0] == '#')
		channel();
	else
		user();
}

void QuitCommand::execute(const Message &msg, int fd)
{
	std::cout << "[DEBUG] Broadcasting to all user\'s channels:" << std::endl;
	std::cout << "user " << fd << " QUIT: ";
	if (!msg.params.empty())
		std::cout << msg.params[0] << std::endl;
	else
		std::cout << "Client Quit" << std::endl;
	std::cout << "[DEBUG] Removing user " << fd << " from channels ETC ETC" << std::endl;
}

void CapCommand::execute(const Message &msg, int fd)
{
	if (msg.params.empty())
		sendResponse("461 CAP :Not enough parameters", fd);
	if (msg.params[0] == "LS")
	{
		sendResponse(":our.host.name CAP * LS :", fd);
	}
	else if (msg.params[0] == "END")
	{
		std::cout << "[DEBUG] Received CAP END, continue..." << std::endl; // continue
	}
	else
		sendResponse("410 CAP :Unsupported subcommand", fd);
}

void WhoisCommand::execute(const Message &msg, int fd)
{
	if (msg.params.empty())
	{
		sendResponse("431 :No nickname given", fd);
		return ;
	}
	std::cout << "[DEBUG] WHOIS is looking for " << msg.params[0] << " from clients:";
	for (auto client : irc->getClients())
	{
		std::cout << " " << client.first;
	}
	std::cout << std::endl;
	// if found
	// sendResponse("311 nick user host * :real name", fd)
	// sendResponse("312 nick servername :server info", fd)
	// sendResponse("318 nick :End of WHOIS list", fd)
}

// possible to implement a timeout mechanic when a client doesn't send PING
// for a while
void PingCommand::execute(const Message &msg, int fd)
{
	if (msg.params.empty())
	{
		sendResponse("409 :No origin specified", fd);
		return ;
	}
	std::string response = "PONG ircserv :" + msg.params[0];
	sendResponse(response, fd);
}
