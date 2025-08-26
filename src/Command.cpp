#include "Command.hpp"

static void debugLog(const Message &msg)
{
	std::cout << "Executed " << msg.command << " command with ";
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
		std::cerr << "Nickname must be 1-9 characters long" << std::endl;
		return ;
	}

	std::regex nickname_regex("^[A-Za-z][A-Za-z0-9-_]*&");
	if (!std::regex_match(newNick, nickname_regex))
	{
		std::cerr << "Invalid characters in nickname" << std::endl;
		return ;
	}

	// check for collissions with existing nicknames
	// -> handle it
	// if changing from a previous nickname, broadcast change to others
	std::cout << "Set client nickname to: " << newNick << std::endl;
}

void UserCommand::execute(const Message &msg, int fd)
{
	(void)fd;
	debugLog(msg);
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
	debugLog(msg);
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
		std::cout << "Received END, continue..." << std::endl; // continue
	}
	else
		tempClientSend("410 CAP :Unsupported subcommand", fd);
}
