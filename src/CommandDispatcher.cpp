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
	_handlers["WHO"] = std::make_unique<WhoCommand>();
	_handlers["PING"] = std::make_unique<PingCommand>();
	_handlers["PASS"] = std::make_unique<PassCommand>();
}

/**
 * Search for an installed command handler for the given message and execute
 * @param msg	The full command to execute
 */
bool	CommandDispatcher::dispatch(const std::unique_ptr<Message> &msg, int fd)
{
	try
	{
		if (auto cmd = _handlers.find(msg->command); cmd != _handlers.end())
		{
			if (!irc->checkPassword() &&
				!irc->getClient(fd).isAuthenticated() &&
				msg->command != "PASS" &&
				msg->command != "CAP")
			{
				std::string response("464 ");
				response.append(irc->getClient(fd).getUser()->getNick());
				response.append(" :Password incorrect\r\n");
				send(fd, response.c_str(), response.size(), 0);
				irc->removeClient(fd);
				return false;
			}
			cmd->second->execute(*msg, fd);
			if (msg->command != "QUIT" &&
				!irc->getClient(fd).getUser()->getNick().empty() &&
				!irc->getClient(fd).getUser()->getUser().empty() &&
				!irc->getClient(fd).accessRegistered())
				_welcome(fd);
		}
		else
			_default.execute(*msg, fd);
	}
	catch (std::exception &e)
	{
		std::cerr << "Command dispatcher error: " << e.what() << std::endl;
	}
	return (true);
}

void	CommandDispatcher::_welcome(int fd)
{
	irc->getClient(fd).accessRegistered() = true;
	std::string nick = irc->getClient(fd).getUser()->getNick();
	std::string response = "001 " + nick + " :Welcome to Hive network\r\n";
	send(fd, response.c_str(), response.size(), 0);
	response = "002 " + nick + " :Your hostname was discarded\r\n";
	send(fd, response.c_str(), response.size(), 0);
	response = "003 " + nick + " :This server was started " + irc->getTime() + "\r\n";
	send(fd, response.c_str(), response.size(), 0);
	response = "004 " + nick + " :Your username is " +
		irc->getClient(fd).getUser()->getUser() + "\r\n";
	send(fd, response.c_str(), response.size(), 0);
}
