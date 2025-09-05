#pragma once
#include <memory>
#include <unordered_map>
#include <exception>
#include <iostream>
#include "Command.hpp"
#include "Message.hpp"
#include <queue>

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

		bool	dispatch(std::unique_ptr<Message> &msg, int fd);

	private:
		std::unordered_map<std::string, std::unique_ptr<ICommand>> _handlers;
		UnknownCommand _default;
		std::queue<std::unique_ptr<Message>> _onHold;
		
		void	_processQueue(int fd);
		bool	_nickResolved = false;
};
