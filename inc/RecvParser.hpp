#pragma once
#include <string>
#include <sstream>
#include <optional>
#include <iostream>
#include <queue>
#include <memory>
#include "Message.hpp"

/**
 * @class	RecvParser
 * @brief	A class for parsing data read by recv() into a message queue
 */
class	RecvParser
{
	public:
		RecvParser(std::queue<std::unique_ptr<Message>> &msg_queue);

		std::queue<std::unique_ptr<Message>> &getQueue(void);
		void	feed(const char *read_buf, size_t len);

	private:
		std::string	_buffer;
		std::queue<std::unique_ptr<Message>> &_output;

		RecvParser(void) = delete;

		void	_normalizeNewLines(void);
		void	_parseBuffer(void);
		Message	_parseMessage(const std::string &msg);
};
