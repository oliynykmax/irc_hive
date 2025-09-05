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
	if (send(fd, message.c_str(), message.size(), 0) < 0)
		throw (std::runtime_error("Failed to send respond (send)"));
}

void NickCommand::execute(const Message &msg, int fd)
{
	if (msg.params.empty())
		return sendResponse("431 :No nickname given", fd);

	const std::string &oldNick = irc->getClient(fd).getUser()->getNick();
	std::string	newNick = msg.params[0];
	std::regex nickname_regex("^[A-Za-z][A-Za-z0-9-_]*");
	if (newNick.size() < 1 || newNick.size() > 9 || !std::regex_match(newNick, nickname_regex))
		return sendResponse("432 " + oldNick + " " + newNick + " :Erroneous nickname", fd);
	for (auto client : irc->getClients())
	{
		if (client.second.getUser()->getNick() == newNick)
			return sendResponse("433 * " + newNick + " :Nickname is already in use", fd);
	}
	irc->getClient(fd).getUser()->setNick(newNick);
}

void UserCommand::execute(const Message &msg, int fd)
{
	if (msg.params.size() < 4)
		return sendResponse("461 :Missing parameters", fd);
	if (!irc->getClient(fd).getUser()->getUser().empty())
		return sendResponse("462 " + irc->getClient(fd).getUser()->getNick() +
				" : You may not reregister", fd);
	irc->getClient(fd).getUser()->setUser(msg.params[0]);
	std::string nick = irc->getClient(fd).getUser()->getNick();
	sendResponse("001 " + nick + " :Welcome to Hive network", fd);
	sendResponse("002 " + nick + " :Your host is " + msg.params[1], fd);
	sendResponse("003 " + nick + " :This server was started " + irc->getTime(), fd);
	sendResponse("004 " + nick + " :Your info is " + msg.params[0] + " " + msg.params[3], fd);
}

void JoinCommand::execute(const Message &msg, int fd)
{
	if (msg.params.size() < 1)
		return sendResponse("461 :Missing parameters", fd);
	std::string nick = irc->getClient(fd).getUser()->getNick();
	std::regex channel_regex("^[#][A-Za-z0-9-_]{1,50}*");
	if (!std::regex_match(msg.params[0], channel_regex))
		return sendResponse("403 " + nick + " " + msg.params[0] + " :No such channel", fd);
	Channel &channel = irc->addChannel(msg.params[0]);
	std::string response;
	if (msg.params.size() == 1)
		response = channel.addUser(fd);
	else
		response = channel.addUser(fd, msg.params[1]);
	if (!response.empty())
		return sendResponse(response, fd);
	const std::string &topic = channel.getTopic();
	if (topic.empty())
		sendResponse("331 " + nick + " " + msg.params[0] + " :No topic is set", fd);
	else
		sendResponse("332 " + nick + " " + msg.params[0] + " :" + topic, fd);
	const std::string &names = channel.userList();
	sendResponse("353 " + nick + " @ " + msg.params[0] + " :" + names, fd);
	sendResponse("366 " + nick + " " + msg.params[0] + " :End of NAMES list", fd);
	for (auto user : channel.getUsers())
		sendResponse(":" + nick + " JOIN :" + msg.params[0], user);
	for (auto oper : channel.getOperators())
		sendResponse(":" + nick + " JOIN :" + msg.params[0], oper);
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
		return sendResponse("411 :No recipient given", fd);
	if (msg.params.size() < 2)
		return sendResponse("412 :No text to send", fd);
	if (msg.params[0][0] == '#')
	{
		Channel *ch = irc->getClient(fd).getUser()->getChannel(msg.params[0]);
		if (!ch)
			return sendResponse("442 :You're not on the channel", fd);
		if (!ch->message(fd, msg.params[1], "PRIVMSG"))
			throw (std::runtime_error("Send() failed in PRIVMSG #channel"));
	}
	else
	{
		for (auto client : irc->getClients())
		{
			if (client.second.getUser()->getNick() == msg.params[0])
			{
				std::string message = ":" + client.second.getUser()->getNick() +
					" PRIVMSG " + msg.params[1] + "\r\n";
				send(client.first, message.c_str(), message.size(), 0);
				return ;
			}
		}
		sendResponse("401 :No such nick", fd);
	}
}

void KickCommand::execute(const Message &msg, int fd)
{
	if (msg.params.size() < 2)
		return sendResponse("461 :Missing parameters", fd);
	if (!irc->channelExists(msg.params[0]))
		return sendResponse("403 :No such channel", fd);

	Channel *ch = irc->getClient(fd).getUser()->getChannel(msg.params[0]);
	if (!ch)
		return sendResponse("442 :You're not on that channel", fd);
	if (!ch->getOperators().contains(fd))
		return sendResponse("482 :You're not a channel operator", fd);
	for (auto client : ch->getOperators())
	{
		if (irc->getClient(client).getUser()->getNick() == msg.params[1])
			return sendResponse("482 :You can't kick an operator", fd);
	}

	Client *target = nullptr;
	for (auto client : ch->getUsers())
	{
		if (irc->getClient(client).getUser()->getNick() == msg.params[1])
		{
			target = &irc->getClient(client);
			break ;
		}
	}
	if (!target)
		return sendResponse("441 :They aren't on that channel", fd);

	std::string nick = ":" + irc->getClient(fd).getUser()->getNick();
	std::string response = nick + " KICK " + msg.params[0] + " " + msg.params[1];
	if (msg.params.size() < 3)
		response.append(" " + nick);
	else
		response.append(" :" + msg.params[2]);
	for (auto user : ch->getUsers())
		sendResponse(response, user);
	for (auto user : ch->getOperators())
		sendResponse(response, user);
	ch->kick(fd, target->_fd);
}

void InviteCommand::execute(const Message &msg, int fd)
{
	if (msg.params.size() < 2)
		return sendResponse("461 :Missing parameters", fd);
	Client *target = nullptr;
	for (auto client : irc->getClients())
	{
		if (client.second.getUser()->getNick() == msg.params[0])
		{
			target = &(client.second);
			break ;
		}
	}
	if (!target)
		return sendResponse("401 :No such nick", fd);
	if (target->_fd == fd)
		return sendResponse("443 :You cannot invite yourself", fd);
	Channel *ch = irc->findChannel(msg.params[1]);
	if (ch != nullptr)
	{
		if (!ch->getUsers().contains(fd) && !ch->getOperators().contains(fd))
			return sendResponse("442: You're not on that channel", fd);
		else if (ch->getMode().contains('i') && !ch->getOperators().contains(fd))
			return sendResponse("482 :You're not a channel operator", fd);
		else if (ch->getUsers().contains(target->_fd))
			return sendResponse("443 :User already on channel", fd);
		else if (ch->getOperators().contains(target->_fd))
			return sendResponse("443 :User already on channel", fd);
		else
			ch->invite(target->_fd);
	}
	std::string invitation = ":" + irc->getClient(fd).getUser()->getNick();
	invitation.append(" INVITE " + msg.params[0] + " :" + msg.params[1]);
	sendResponse(invitation, target->_fd);
	sendResponse("341 :Invitation send", fd);
}

void TopicCommand::execute(const Message &msg, int fd)
{
	if (msg.params.empty())
		return sendResponse("461 :Missing parameters", fd);
	if (!irc->channelExists(msg.params[0]))
		return sendResponse("403 :Channel doesn't exist", fd);
	Channel *ch = irc->getClient(fd).getUser()->getChannel(msg.params[0]);
	if (!ch)
		return sendResponse("442 :You're not on the channel", fd);
	if (msg.params.size() < 2)
	{
		const std::string &nick = irc->getClient(fd).getUser()->getNick();
		const std::string &topic = ch->getTopic();
		if (topic.empty())
			return sendResponse("331 " + nick + " " + msg.params[0] + " :No topic is set", fd);
		return sendResponse("332 " + nick + " " + msg.params[0] + " :" + topic, fd);
	}
	if (ch->getMode().contains('t') && !ch->getOperators().contains(fd))
		return sendResponse("482 :You're not a channel operator", fd);
	ch->setTopic(fd, msg.params[1]);
}

void ModeCommand::execute(const Message &msg, int fd)
{
	auto channel = [&]()
	{
		Channel *ch = irc->getClient(fd).getUser()->getChannel(msg.params[0]);
		Channel backup = *ch;
		if (!ch)
			return sendResponse("442 :You're not on the channel", fd);
		if (msg.params.size() < 2)
		{
			std::string response("324 " + msg.params[0] + " ");
			std::string modes("+");
			for (char c : ch->getMode())
				modes.push_back(c);
			response.append(modes);
			return sendResponse(response, fd);
		}
		if (!ch->getOperators().contains(fd))
			return sendResponse("482 :Channel operator privileges required", fd);
		std::string input = msg.params[1];
		std::string seen;
		for (auto c = seen.begin(); c != seen.end(); ++c)
		{
			if (*c != '-' && *c != '+')
			{
				if (seen.find(*c) != std::string::npos)
					return sendResponse("472 :Unknown mode", fd);
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
					return sendResponse("472 :Unknown mode", fd);
			}
			else
				return sendResponse("472 :Unknown mode", fd);
		}
		std::string supported = "itkol";
		std::string requireParam = "klo";
		size_t paramsNeeded = 2;
		for (auto c = enable.begin(); c != enable.end(); ++c)
		{
			if ((supported.find(*c) == std::string::npos)
				|| (std::find(c + 1, enable.end(), *c) != enable.end()))
				return sendResponse("472 :Unknown mode", fd);
			if (requireParam.find(*c) != std::string::npos)
				paramsNeeded++;
		}
		for (auto c = disable.begin(); c != disable.end(); ++c)
		{
			if ((supported.find(*c) == std::string::npos)
				|| (std::find(c + 1, disable.end(), *c) != disable.end()))
				return sendResponse("472 :Unknown mode", fd);
		}
		if (msg.params.size() < paramsNeeded)
			return sendResponse("461 :Need more parameters", fd);
		int index = 2;
		try
		{
			for (auto c : enable)
			{
				if (c == 'k')
					ch->setPassword(msg.params[index++]);
				if (c == 'l')
				{
					try
					{
						ch->setLimit(std::stoul(msg.params[index++]));
					}
					catch (std::exception &e)
					{
						sendResponse("472 :Unknown mode", fd);
						throw ;
					}
				}
				if (c == 'o')
				{
					auto user = ch->getUsers().begin();
					while (user != ch->getUsers().end())
					{
						if (irc->getClient(*user).getUser()->getNick() ==
								msg.params[index])
						{
							ch->makeOperator(*user);
							index++;
							break ;
						}
						++user;
					}
					if (user == ch->getUsers().end())
					{
						sendResponse("441 " +
								irc->getClient(fd).getUser()->getNick() +
								" " + msg.params[0] +
								" :They aren't on that channel", fd);
						throw (std::runtime_error(""));
					}

				}
			}
		}
		catch (std::exception &e)
		{
			*ch = backup;
			return ;
		}
		ch->setMode(enable);
		ch->unsetMode(disable);
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
		return sendResponse("461 :Need more parameters", fd);
	if (msg.params[0][0] == '#')
		channel();
	else
		user();
}

void QuitCommand::execute(const Message &msg, int fd)
{
	if (!msg.params.empty())
		irc->getClient(fd).getUser()->quit(fd, msg.params[0]);
	else
		irc->getClient(fd).getUser()->quit(fd, "Client quit");
}

void CapCommand::execute(const Message &msg, int fd)
{
	if (msg.params.empty())
		return sendResponse("461 CAP :Not enough parameters", fd);
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
	(void)msg;
	std::string nick = irc->getClient(fd).getUser()->getNick();
	sendResponse("318 " + nick + " :End of WHOIS list", fd);
}

void WhoCommand::execute(const Message &msg, int fd)
{
	if (msg.params.empty())
		sendResponse("461 :Not enough parameters", fd);
	Channel *ch = irc->getClient(fd).getUser()->getChannel(msg.params[0]);
	if (!ch)
		return sendResponse("442 :You're not on the channel", fd);
	const std::string &nick = irc->getClient(fd).getUser()->getNick();
	for (auto id : ch->getUsers())
	{
		const User *user = irc->getClient(id).getUser();
		std::string who("352 " + nick + " " + msg.params[0]);
		who.append(" " + user->getUser() + " hostname ircserv " + user->getNick() + " H");
		who.append("\r\n");
		send(fd, who.c_str(), who.size(), 0);
	}
	for (auto id : ch->getOperators())
	{
		const User *user = irc->getClient(id).getUser();
		std::string who("352 " + nick + " " + msg.params[0]);
		who.append(" " + user->getUser() + " hostname ircserv " + user->getNick() + " H");
		who.append("@\r\n");
		send(fd, who.c_str(), who.size(), 0);
	}
	std::string endofwho("315 " + nick + " " +
		msg.params[0] + " :End of /WHO list.\r\n");
	sendResponse(endofwho, fd);
}

void PingCommand::execute(const Message &msg, int fd)
{
	if (msg.params.empty())
		return sendResponse("409 :No origin specified", fd);
	std::string response = "PONG ircserv :" + msg.params[0];
	sendResponse(response, fd);
}

void PassCommand::execute(const Message &msg, int fd)
{
	if (irc->checkPassword())
		return ;
	else if (!msg.params.empty() && irc->checkPassword(msg.params[0]))
	{
		irc->getClient(fd).authenticate();
		return ;
	}
	else
	{
		sendResponse("464 " + irc->getClient(fd).getUser()->getNick() + " :Incorrect password", fd);
		irc->removeClient(fd);
	}
}

void UnknownCommand::execute(const Message &msg, int fd)
{
	debugLog(msg);
	sendResponse("421 :Unknown command", fd);
}
