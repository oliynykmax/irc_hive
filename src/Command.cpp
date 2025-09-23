#include "Command.hpp"




static void debugLog(const Message &msg)
{
	std::cerr << "Received unsupported command " << msg.command << " with ";
	if (msg.prefix)
		std::cerr << "prefix: " << *msg.prefix << ", ";
	std::cerr << "params:";
	for (auto param : msg.params)
		std::cerr << " " << param;
	std::cerr << std::endl;
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
		return sendResponse(E431, fd);
	const std::string &oldNick = NICK;
	std::string	newNick = PARAM;
	User& usr = USER(fd);
	std::regex nickname_regex("^[A-Za-z][A-Za-z0-9-_]*");
	if (newNick.size() < 1 || newNick.size() > 9 || not std::regex_match(newNick, nickname_regex))
		return sendResponse(E432, fd);
	for (auto client : irc->getClients())
		if (client.second->getUser().getNick() == newNick)
			return sendResponse(E433, fd);
	sendResponse(PREFIX + " NICK :" + newNick, fd);
	usr.setNick(fd, newNick);

}

void UserCommand::execute(const Message &msg, int fd)
{
	if (msg.params.size() < 4)
		return sendResponse(E461, fd);
	if (not USER(fd).getUser().empty())
		return sendResponse(E462, fd);
	USER(fd).setUser(PARAM);
	USER(fd).setHost(PARAM1);
}

void JoinCommand::execute(const Message &msg, int fd)
{
	if (msg.params.size() < 1)
		return sendResponse(E461, fd);
	else if (PARAM.empty())
		return ;
	std::regex channel_regex("^[#][A-Za-z0-9-_]{1,50}*");
	if (!std::regex_match(PARAM, channel_regex))
		return sendResponse(E403REV2, fd);
	Channel &channel = irc->addChannel(PARAM);
	std::string response;
	if (msg.params.size() == 1)
		response = channel.addUser(fd);
	else
		response = channel.addUser(fd, PARAM1);
	if (!response.empty())
		return sendResponse(response, fd);
	const std::string &topic = channel.getTopic();
	if (topic.empty())
		sendResponse(R331, fd);
	else
		sendResponse(R332, fd);
	const std::string &names = channel.userList();
	sendResponse(R353, fd);
	sendResponse(R366, fd);
	std::string prefix = PREFIX;
	for (auto user : channel.getUsers())
		sendResponse(prefix + " JOIN :" + PARAM, user);
	for (auto oper : channel.getOperators())
		sendResponse(prefix + " JOIN :" + PARAM, oper);
}

void PartCommand::execute(const Message &msg, int fd)
{
	if (msg.params.size() < 1)
		return sendResponse(E461, fd);
	Client *client = irc->getClient(fd);
	std::regex channel_regex("^[#][A-Za-z0-9-_]{1,50}*");
	if (!std::regex_match(PARAM, channel_regex))
		return sendResponse(E403REV2, fd);
	Channel *ch = client->getUser().getChannel(PARAM);
	if (!ch)
		return sendResponse(E422, fd);
	std::string response = PARAM;
	if (msg.params.size() > 1)
		response.append(" :" + PARAM1);
	ch->removeUser(fd, response, "PART");
	sendResponse(PREFIX + " PART " + response, fd);
}

void PrivmsgCommand::execute(const Message &msg, int fd)
{
	if (msg.params.empty())
		return sendResponse(E411, fd);
	if (msg.params.size() < 2)
		return sendResponse(E412, fd);
	if (PARAM[0] == '#')
	{
		Channel *ch = USER(fd).getChannel(PARAM);
		if (not ch)
			return sendResponse(E442, fd);
		if (not ch->message(fd, PARAM1, "PRIVMSG"))
			throw (std::runtime_error("Send() failed in PRIVMSG #channel"));
	}
	else
	{
		for (auto client : irc->getClients())
			if (client.second->getUser().getNick() == PARAM)
				return sendResponse(PRIVMSG, client.first);
		sendResponse(E401, fd);
	}
}



void KickCommand::execute(const Message &msg, int fd)
{
	if (msg.params.size() < 2)
		return sendResponse(E461, fd);
	if (not irc->channelExists(PARAM))
		return sendResponse(E403, fd);
	Channel *ch = irc->getClient(fd)->getUser().getChannel(PARAM);
	if (not ch)
		return sendResponse(E442, fd);
	if (not ch->getOperators().contains(fd))
		return sendResponse(E482, fd);
	for (auto client : ch->getOperators())
		if (USER(client).getNick() == PARAM1)
			return sendResponse(E481, fd);
	Client *target = nullptr;
	for (auto client : ch->getUsers())
		if (USER(client).getNick() == PARAM1)
		{
			target = irc->getClient(client);
			break ;
		}
	if (not target)
		return sendResponse(E441, fd);
	std::string nick = ":" + NICK;
	std::string response = PREFIX + " KICK " + PARAM + " " + PARAM1;
	if (msg.params.size() < 3)
		response.append(" " + nick);
	else
		response.append(" :" + PARAM2);
	sendResponse(response, fd);
	ch->message(fd, response);
	ch->kick(fd, target->_fd);
}

void InviteCommand::execute(const Message &msg, int fd)
{
	if (msg.params.size() < 2)
		return sendResponse(E461, fd);
	Client *target = nullptr;
	for (auto client : irc->getClients())
		if (client.second->getUser().getNick() == PARAM)
		{
			target = client.second.get();
			break ;
		}
	if (not target)
		return sendResponse(E401, fd);
	if (target->_fd == fd)
		return sendResponse(E443, fd);
	Channel *ch = irc->findChannel(PARAM1);
	if (ch)
	{
		if (not ch->getUsers().contains(fd) && not ch->getOperators().contains(fd))
			return sendResponse(E442, fd);
		else if (ch->getMode().contains('i') && not ch->getOperators().contains(fd))
			return sendResponse(E482, fd);
		else if (ch->getUsers().contains(target->_fd) ||
				ch->getOperators().contains(target->_fd))
			return sendResponse(E443, fd);
		else
			ch->invite(target->_fd);
	}
	std::string invitation = PREFIX + " INVITE " + PARAM + " :" + PARAM1;
	sendResponse(invitation, target->_fd);
	sendResponse(R341, fd);
}

void TopicCommand::execute(const Message &msg, int fd)
{
	if (msg.params.empty())
		return sendResponse(E461, fd);
	if (not irc->channelExists(PARAM))
		return sendResponse(E403, fd);
	Channel *ch = irc->getClient(fd)->getUser().getChannel(PARAM);
	if (not ch)
		return sendResponse(E442, fd);
	if (msg.params.size() < 2)
	{
		const std::string &topic = ch->getTopic();
		if (topic.empty())
			return sendResponse(R331, fd);
		return sendResponse(R332, fd);
	}
	if (ch->getMode().contains('t') && not ch->getOperators().contains(fd))
		return sendResponse(E482, fd);
	ch->setTopic(fd, PARAM1);
}

constexpr std::string supported = "itkol", required = "klo";

void ModeCommand::execute(const Message &msg, int fd)
{
	auto channel = [&]() {
		Channel *ch = irc->getClient(fd)->getUser().getChannel(PARAM);
		if (!ch)
			return sendResponse(E442, fd);
		if (msg.params.size() < 2) {
			sendResponse(R324, fd);
			return sendResponse(R329, fd);
		} else if (!ch->getOperators().contains(fd))
			return sendResponse(E482, fd);
		std::string input = PARAM1, enable, disable;
		bool plus, valid = true;
		for (auto c = input.begin(); c != input.end(); )
			if ((*c == '+'|| *c == '-') && valid) {
				plus = (*c == '+');
				++c;
				for (valid = false; c != input.end() && std::isalpha(*c); c++) {
					plus ? enable += *c : disable += *c;
					valid = true;
				}
			} else
				return sendResponse(E472, fd);
		size_t paramsNeeded = 2;
		for (auto c = enable.begin(); c != enable.end(); ++c)
			if ((supported.find(*c) == std::string::npos)
				|| (std::find(c + 1, enable.end(), *c) != enable.end()))
				return sendResponse(E472, fd);
			else if (required.find(*c) != std::string::npos)
				paramsNeeded++;
		for (auto c = disable.begin(); c != disable.end(); ++c)
			if ((supported.find(*c) == std::string::npos)
				|| (std::find(c + 1, disable.end(), *c) != disable.end()))
				return sendResponse(E472, fd);
		if (msg.params.size() < paramsNeeded)
			return sendResponse(E461, fd);
		Channel backup = *ch;
		int index = 2;
		for (auto c : enable)
			if (c == 'k') {
				ch->setPassword(msg.params[index++]);
			} else if (c == 'l' && not ch->setLimit(msg.params[index++])) {
				*ch = backup;
				return ;
			} else if (c == 'o' && not ch->makeOperator(fd, msg.params[index++])) {
				sendResponse(E441, fd);
				*ch = backup;
				return ;
			}
		if (disable.empty() && enable == "o")
			return ;
		ch->setMode(enable);
		ch->unsetMode(disable);
		ch->message(-1, R324);
	};

	auto user = [&]() {
		if (PARAM != NICK)
			sendResponse(E502, fd);
		return ;
	};

	if (msg.params.size() < 1)
		return sendResponse(E461, fd);
	PARAM[0] == '#' ? channel() : user();
}

void QuitCommand::execute(const Message &msg, int fd)
{
	if (!msg.params.empty())
		USER(fd).quit(fd, PARAM);
	else
		USER(fd).quit(fd, "Client quit");
}

void CapCommand::execute(const Message &msg, int fd)
{
	if (msg.params.empty())
		return sendResponse(CAP461, fd);
	if (PARAM == "LS")
		return sendResponse(CAP, fd);
	else if (PARAM == "END")
		return ;
	else
		return sendResponse(CAP410, fd);
}

void WhoisCommand::execute(const Message &msg, int fd)
{
	(void)msg;
	sendResponse(R318, fd);
}

void WhoCommand::execute(const Message &msg, int fd)
{
	if (msg.params.empty())
		sendResponse(E461, fd);
	Channel *ch = irc->getClient(fd)->getUser().getChannel(PARAM);
	if (not ch)
		return sendResponse(E442, fd);
	const std::string &nick = NICK;
	for (auto id : ch->getUsers())
	{
		const User& user = USER(id);
		sendResponse(R352, fd);
	}
	for (auto id : ch->getOperators())
	{
		const User& user = USER(id);
		sendResponse(R352 + " @", fd);
	}
	sendResponse(R315, fd);
}

void PingCommand::execute(const Message &msg, int fd)
{
	if (msg.params.empty())
		return sendResponse(E409, fd);
	std::string response = "PONG localhost :" + PARAM;
	sendResponse(response, fd);
}

void PassCommand::execute(const Message &msg, int fd)
{
	if (irc->checkPassword())
		return ;
	else if (!msg.params.empty() && irc->checkPassword(PARAM))
		return irc->getClient(fd)->authenticate();
	sendResponse(E464, fd);
	irc->removeClient(fd);
}

void UnknownCommand::execute(const Message &msg, int fd)
{
	debugLog(msg);
	sendResponse(E421, fd);
}
