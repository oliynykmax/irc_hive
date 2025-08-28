#include "CommandDispatcher.hpp"

/**
 * CommandDispatcher installs seperate handlers for each command at construction
 */
CommandDispatcher::CommandDispatcher(void)
{
	_handlers["NICK"] = std::make_unique<NickCommand>();
	_handlers["USER"] = std::make_unique<UserCommand>();
	_handlers["JOIN"] = std::make_unique<JoinCommand>();
	_handlers["PART"] = std::make_unique<PartCommand>();
	_handlers["PRIVMSG"] = std::make_unique<PrivmsgCommand>();
	_handlers["KICK"] = std::make_unique<KickCommand>();
	_handlers["INVITE"] = std::make_unique<InviteCommand>();
	_handlers["TOPIC"] = std::make_unique<TopicCommand>();
	_handlers["MODE"] = std::make_unique<ModeCommand>();
	_handlers["QUIT"] = std::make_unique<QuitCommand>();
	_handlers["CAP"] = std::make_unique<CapCommand>();
	_handlers["WHOIS"] = std::make_unique<WhoisCommand>();
	_handlers["PING"] = std::make_unique<PingCommand>();
}

/**
 * Search for an installed command handler for the given message and execute
 * @param msg	The full command to execute
 */
void	CommandDispatcher::dispatch(const std::unique_ptr<Message> &msg, int fd)
{
	try
	{
		if (auto cmd = _handlers.find(msg->command); cmd != _handlers.end())
			cmd->second->execute(*msg, fd);
		else
			std::cerr << "Unsupported command: " << msg->command << std::endl;
	}
	catch (std::exception &e)
	{
		std::cerr << "Command dispatcher error: " << e.what() << std::endl;
	}
}
