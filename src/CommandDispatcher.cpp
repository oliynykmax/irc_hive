#include "CommandDispatcher.hpp"

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
}

void	CommandDispatcher::dispatch(const std::unique_ptr<Message> &msg)
{
	try
	{
		if (auto cmd = _handlers.find(msg->command); cmd != _handlers.end())
			cmd->second->execute(*msg);
		else
			std::cerr << "Unsupported command: " << msg->command << std::endl;
	}
	catch (std::exception &e)
	{
		std::cerr << "Command dispatcher error: " << e.what() << std::endl;
	}
}
