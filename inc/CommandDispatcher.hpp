#pragma once
#include <memory>
#include <unordered_map>
#include <exception>
#include <iostream>
#include "Command.hpp"
#include "Message.hpp"

class	CommandDispatcher
{
	public:
		CommandDispatcher(void);

		void	dispatch(const std::unique_ptr<Message> &msg);

	private:
		std::unordered_map<std::string, std::unique_ptr<ICommand>> _handlers;
};
