#include "Command.hpp"

void NickCommand::execute(const Message &msg)
{
	std::cout << "Executed NICK command with ";
	if (msg.prefix)
		std::cout << "prefix: " << *msg.prefix << ", ";
	std::cout << "params:";
	for (auto param : msg.params)
		std::cout << " " << param;
	std::cout << std::endl;
}

void UserCommand::execute(const Message &msg)
{
	std::cout << "Executed USER command with ";
	if (msg.prefix)
		std::cout << "prefix: " << *msg.prefix << ", ";
	std::cout << "params:";
	for (auto param : msg.params)
		std::cout << " " << param;
	std::cout << std::endl;
}

void JoinCommand::execute(const Message &msg)
{
	std::cout << "Executed JOIN command with ";
	if (msg.prefix)
		std::cout << "prefix: " << *msg.prefix << ", ";
	std::cout << "params:";
	for (auto param : msg.params)
		std::cout << " " << param;
	std::cout << std::endl;
}

void PartCommand::execute(const Message &msg)
{
	std::cout << "Executed PART command with ";
	if (msg.prefix)
		std::cout << "prefix: " << *msg.prefix << ", ";
	std::cout << "params:";
	for (auto param : msg.params)
		std::cout << " " << param;
	std::cout << std::endl;
}

void PrivmsgCommand::execute(const Message &msg)
{
	std::cout << "Executed PRIVMSG command with ";
	if (msg.prefix)
		std::cout << "prefix: " << *msg.prefix << ", ";
	std::cout << "params:";
	for (auto param : msg.params)
		std::cout << " " << param;
	std::cout << std::endl;
}

void KickCommand::execute(const Message &msg)
{
	std::cout << "Executed KICK command with ";
	if (msg.prefix)
		std::cout << "prefix: " << *msg.prefix << ", ";
	std::cout << "params:";
	for (auto param : msg.params)
		std::cout << " " << param;
	std::cout << std::endl;
}

void InviteCommand::execute(const Message &msg)
{
	std::cout << "Executed INVITE command with ";
	if (msg.prefix)
		std::cout << "prefix: " << *msg.prefix << ", ";
	std::cout << "params:";
	for (auto param : msg.params)
		std::cout << " " << param;
	std::cout << std::endl;
}

void TopicCommand::execute(const Message &msg)
{
	std::cout << "Executed TOPIC command with ";
	if (msg.prefix)
		std::cout << "prefix: " << *msg.prefix << ", ";
	std::cout << "params:";
	for (auto param : msg.params)
		std::cout << " " << param;
	std::cout << std::endl;
}

void ModeCommand::execute(const Message &msg)
{
	std::cout << "Executed MODE command with ";
	if (msg.prefix)
		std::cout << "prefix: " << *msg.prefix << ", ";
	std::cout << "params:";
	for (auto param : msg.params)
		std::cout << " " << param;
	std::cout << std::endl;
}

void QuitCommand::execute(const Message &msg)
{
	std::cout << "Executed QUIT command with ";
	if (msg.prefix)
		std::cout << "prefix: " << *msg.prefix << ", ";
	std::cout << "params:";
	for (auto param : msg.params)
		std::cout << " " << param;
	std::cout << std::endl;
}

void CapCommand::execute(const Message &msg)
{
	if (msg.params.empty())
		throw (std::runtime_error("461 CAP :Not enough parameters"));
	if (msg.params[0] == "LS")
	{
		std::cout << ":server.name CAP * LS :" << std::endl;
	}
	else if (msg.params[0] == "END")
	{
		std::cout << "Received END, continue..." << std::endl;
	}
	else
		throw (std::runtime_error("410 CAP :Unsupported subcommand"));
	std::cout << "Executed CAP command with ";
	if (msg.prefix)
		std::cout << "prefix: " << *msg.prefix << ", ";
	std::cout << "params:";
	for (auto param : msg.params)
		std::cout << " " << param;
	std::cout << std::endl;
}
