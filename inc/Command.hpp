#pragma once
#include "Message.hpp"
#include <iostream>

/**
 * @class	ICommand
 * @brief	Interface for all IRC commands
 *
 * Each concrete Command class overrides the execute() function to do what they
 * are supposed to do with the parsed Message struct received as an argument.
 */
class	ICommand
{
	public:
		virtual ~ICommand(void){};
		virtual void	execute(const Message &msg) = 0;
};

class	NickCommand : public ICommand
{
	public:
		void	execute(const Message &msg) override;
};

class	UserCommand : public ICommand
{
	public:
		void	execute(const Message &msg) override;
};

class	JoinCommand : public ICommand
{
	public:
		void	execute(const Message &msg) override;
};

class	PartCommand : public ICommand
{
	public:
		void	execute(const Message &msg) override;
};

class	PrivmsgCommand : public ICommand
{
	public:
		void	execute(const Message &msg) override;
};

class	KickCommand : public ICommand
{
	public:
		void	execute(const Message &msg) override;
};

class	InviteCommand : public ICommand
{
	public:
		void	execute(const Message &msg) override;
};

class	TopicCommand : public ICommand
{
	public:
		void	execute(const Message &msg) override;
};

class	ModeCommand : public ICommand
{
	public:
		void	execute(const Message &msg) override;
};

class	QuitCommand : public ICommand
{
	public:
		void	execute(const Message &msg) override;
};
