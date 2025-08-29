#include "Command.hpp"

static void debugLog(const Message &msg)
{
	std::cout << "[DEBUG] Received unsupported command " << msg.command << " with ";
	if (msg.prefix)
		std::cout << "prefix: " << *msg.prefix << ", ";
	std::cout << "params:";
	for (auto param : msg.params)
		std::cout << " " << param;
	std::cout << std::endl;
}

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
		if (client.second.getUser()->getNick() == newNick)
		{
			sendResponse("433 :Nickname is already in use", fd);
			return ;
		}
	}
	irc->getClient(fd).getUser()->setNick(newNick);
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
	if (msg.params.size() < 1)
	{
		sendResponse("461 :Missing parameters", fd);
		return ;
	}
	std::regex channel_regex("^[#][A-Za-z0-9-_]{1,50}*");
	if (!std::regex_match(msg.params[0], channel_regex))
	{
		sendResponse("403 <nick> " + msg.params[0] + " :No such channel", fd);
		return ;
	}
	// if user is on the channel already
	//	sendResponse("443 :You are already on the channel", fd);
	// if user is on too many channels i guess?
	//	sendResponse("405 :You have joined too many channels", fd);
	// if channel has limit and is full
	//	sendResponse("471 :Cannot join channel (Channel is full)", fd);
	// if channel is invite only
	//	sendResponse("473 :Cannot join an invite only channel", fd);
	// if channel has password and you put in a wrong one
	//	sendResponse("475 :Incorrect channel key", fd);

	// update client & server
	// sendResponse("331 <nick> <channel> :No topic is set", fd);
	// sendResponse("332 <nick> <channel> :<topic>", fd);
	// sendResponse("353 <nick> @ <channel> :<nick1> <nick2> <nick3>...", fd);
	// sendResponse("366 <nick> <channel> :End of NAMES list", fd);
	std::cout << "[DEBUG] User " << fd << " joined/created channel " << msg.params[0] << std::endl;
	// for (client : channel.clients())
	//	sendResponse(":<nick>!<user>@<host> JOIN :<channel>", client.fd());
}

void PartCommand::execute(const Message &msg, int fd)
{
	if (msg.params.size() < 1)
	{
		sendResponse("461 :Missing parameters", fd);
		return ;
	}
	std::regex channel_regex("^[#][A-Za-z0-9-_]{1,50}*");
	if (!std::regex_match(msg.params[0], channel_regex))
	{
		sendResponse("403 <nick> " + msg.params[0] + " :No such channel", fd);
		return ;
	}
	// if user not on channel
	//	sendResponse("422 :You're not on that channel", fd);

	// success
	// update client & server
	std::cout << "[DEBUG] User " << fd << " left channel " << msg.params[0] << std::endl;
	// for (client : channel.clients())
	//	sendResponse(":<nick>!<user>@<host> PART <channel> :<message>", fd);
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
	if (msg.params.size() < 2)
	{
		sendResponse("461 :Missing parameters", fd);
		return ;
	}
	// if channel doesn't exist
	//	sendResponse("403 :No such channel", fd);
	// if user not on channel
	//	sendResponse("442 :You're not on that channel", fd);
	// if sender not channel op
	//	sendResponse("482 :You're not a channel operator", fd);
	// if target not on channel
	//	sendResponse("441 :They aren't on that channel", fd);
	// if target is op
	//	sendResponse("482 :You can't kick an operator", fd);

	// success
	//	sendResponse("KICK <channel> <target> :message", fd);
	std::cout << "[DEBUG] User " << fd << " succesfully kicked ";
	std::cout << msg.params[0] <<  " from " << msg.params[1] << std::endl;
}

void InviteCommand::execute(const Message &msg, int fd)
{
	if (msg.params.size() < 2)
	{
		sendResponse("461 :Missing parameters", fd);
		return ;
	}
	// if nick not recognized
	//	sendResponse("401 :No such nick", fd);
	// if channel not found
	//	sendResponse("403 :No such channel", fd);
	// if not channel op
	//	sendResponse("482 :You're not a channel operator", fd);
	// if user already on channel
	//	sendResponse("443 :User already on channel", fd);
	// if inviting yourself
	//	sendResponse("443 :You cannot invite yourself", fd);
	// success
	//	sendResponse("341 :Invitation send", fd);
	std::cout << "[DEBUG] User " << fd << " succesfully invited ";
	std::cout << msg.params[0] << " to " << msg.params[1] << std::endl;
}

void TopicCommand::execute(const Message &msg, int fd)
{
	if (msg.params.empty())
	{
		sendResponse("461 :Missing parameters", fd);
		return ;
	}
	// if channel doesn't exist
	//  sendResponse("403 :Channel doesn't exist", fd);
	// if user not on channel
	//  sendResponse("442 :You're not on the channel", fd);
	if (msg.params.size() < 2)
	{
		// sendResponse("332 <nick> <channel> :Current topic: <topic>", fd);
		// sendResponse("331 <nck> <channel> :No topic is set", fd);
		std::cout << "[DEBUG] Retrieving topic for " << msg.params[0] << std::endl;
		return ;
	}
	// if channel has +t and not operator
	//  sendResponse("482 :You're not a channel operator", fd);
	std::cout << "[DEBUG] Setting topic \"" << msg.params[1] << "\" for " << msg.params[0] << std::endl;
}

void ModeCommand::execute(const Message &msg, int fd)
{
	//sendResponse("472 :Unknown mode", fd);
	auto channel = [&]()
	{
		// if user not operator on channel;
		// sendResponse("482 :Channel operator privileges required", fd);
		if (msg.params.size() < 2)
		{
			sendResponse("461 :Need more parameters", fd);
			return ;
		}
		std::string input = msg.params[1];
		std::string seen;
		for (auto c = seen.begin(); c != seen.end(); ++c)
		{
			if (*c != '-' && *c != '+')
			{
				if (seen.find(*c) != std::string::npos)
				{
					sendResponse("472 :Unknown mode", fd);
					return ;
				}
				seen += *c;
			}
		}
		std::string enable;
		std::string disable;
		bool plus;
		bool valid;
		auto c = input.begin();
		while (c != input.end())
		{
			if (*c == '+'|| *c == '-')
			{
				plus = (*c == '+');
				++c;
				valid = false;
				while (c != input.end() && std::isalpha(*c))
				{
					valid = true;
					if (plus)
						enable += *c;
					else
						disable += *c;
					++c;
				}
				if (!valid)
				{
					sendResponse("472 :Unknown mode", fd);
					return ;
				}
			}
			else
			{
				sendResponse("472 :Unknown mode", fd);
				return ;
			}
		}
		std::string supported = "itkol";
		std::string requireParam = "kl";
		size_t paramsNeeded = 2;
		for (char c : enable)
		{
			if (supported.find(c) == std::string::npos)
			{
				sendResponse("472 :Unknown mode", fd);
				return ;
			}
			if (requireParam.find(c) != std::string::npos)
				paramsNeeded++;
		}
		for (char c : disable)
		{
			if (supported.find(c) == std::string::npos)
			{
				sendResponse("472 :Unknown mode", fd);
				return ;
			}
		}
		if (msg.params.size() < paramsNeeded)
		{
			sendResponse("461 :Need more parameters", fd);
			return ;
		}
		std::cout << "[DEBUG] Enabled channel modes " << enable << " for " << msg.params[0] << std::endl;
		std::cout << "[DEBUG] Disabled channel modes " << disable << " for " << msg.params[0] << std::endl;
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
	irc->removeClient(fd);
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

void PassCommand::execute(const Message &msg, int fd)
{
	if (irc->checkPassword()) {
		return ;
	} else if (!msg.params.empty() && irc->checkPassword(msg.params[0])) {
		return ;
	} else {
		sendResponse("464 :Incorrect password", fd);
		irc->removeClient(fd);
	}
}

void UnknownCommand::execute(const Message &msg, int fd)
{
	debugLog(msg);
	sendResponse("421 :Unknown command", fd);
}
