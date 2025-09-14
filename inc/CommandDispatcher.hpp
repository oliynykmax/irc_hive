#pragma once
#include <memory>
#include <unordered_map>
#include <exception>
#include <iostream>
#include "Command.hpp"
#include "Message.hpp"

class ICommand;
class UnknownCommand;

/**
 * @class	CommandDispatcher
 * @brief	A class for executing commands received from parser
 *
 * CommandDispatcher contains an unordered_map (dictionary) of command handlers
 * which it can call for any parsed Message struct passed to dispatch().
 */
class	CommandDispatcher
{
	public:
		CommandDispatcher(void);

		bool	dispatch(const std::unique_ptr<Message> &msg, int fd);

	private:
		std::unordered_map<std::string, std::unique_ptr<ICommand>> _handlers;
		std::unique_ptr<UnknownCommand> _default;

		void	_welcome(int fd);
};
