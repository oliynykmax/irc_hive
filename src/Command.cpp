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
static void	tempClientSend(std::string message, int fd)
{
	message.append("\r\n");
	send(fd, message.c_str(), message.size(), 0);
}

void NickCommand::execute(const Message &msg, int fd)
{
	if (msg.params.empty())
	{
		tempClientSend("431 :No nickname given", fd);
		return ;
	}

	std::string	newNick = msg.params[0];

	if (newNick.size() < 1 || newNick.size() > 9)
	{
		tempClientSend("432 :Erroneous nickname", fd);
		return ;
	}

	std::regex nickname_regex("^[A-Za-z][A-Za-z0-9-_]*");
	if (!std::regex_match(newNick, nickname_regex))
	{
		tempClientSend("432 :Erroneous nickname", fd);
		return ;
	}

	// check for collissions with existing nicknames
	// -> handle it
	//		tempClientSend("433 :Nickname is already in use", fd);
	// if changing from a previous nickname, broadcast change to others
	std::cout << "[DEBUG] Set client nickname to: " << newNick << std::endl;
}

void UserCommand::execute(const Message &msg, int fd)
{
	debugLog(msg);
	if (msg.params.size() < 4)
	{
		tempClientSend("461 :Missing parameters", fd);
		return ;
	}
	// if registered
	// tempClientSend("462 :Already registered", fd);
	// register
	tempClientSend("001 yournick :Welcome to IRC network", fd);
	tempClientSend("002 yournick :Your host is hostname", fd);
	tempClientSend("003 yournick :This server was created moments ago", fd);
	tempClientSend("004 yournick :Your info is this and that", fd);
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
	(void)fd;
	debugLog(msg);
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
	(void)fd;
	debugLog(msg);
}

void QuitCommand::execute(const Message &msg, int fd)
{
	(void)fd;
	std::cout << "[DEBUG] Broadcasting to all user\'s channels:" << std::endl;
	std::cout << "user QUIT: ";
	if (!msg.params.empty())
		std::cout << msg.params[0] << std::endl;
	else
		std::cout << "Client Quit" << std::endl;
	std::cout << "[DEBUG] Removing user from channels ETC ETC" << std::endl;
}

void CapCommand::execute(const Message &msg, int fd)
{
	if (msg.params.empty())
		tempClientSend("461 CAP :Not enough parameters", fd);
	if (msg.params[0] == "LS")
	{
		tempClientSend(":our.host.name CAP * LS :", fd);
	}
	else if (msg.params[0] == "END")
	{
		std::cout << "[DEBUG] Received CAP END, continue..." << std::endl; // continue
	}
	else
		tempClientSend("410 CAP :Unsupported subcommand", fd);
}

void WhoisCommand::execute(const Message &msg, int fd)
{
	(void)fd;
	debugLog(msg);
}

void PingCommand::execute(const Message &msg, int fd)
{
	(void)fd;
	debugLog(msg);
}
