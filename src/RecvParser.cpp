#include "RecvParser.hpp"

RecvParser::RecvParser(std::queue<std::unique_ptr<Message>> &msg_queue)
	: _output(msg_queue){}

/**
 *	Feed the recv() buffer into std::string buffer
 *	@param	read_buf	The buffer to read
 *	@param	len			Length of the buffer
 */
void	RecvParser::feed(const char *read_buf, size_t len)
{
	_buffer.append(read_buf, len);
	_normalizeNewLines();
	_parseBuffer();
}

/**
 *	Make all newlines conform to IRC protocol to make evals take less time
 */
void	RecvParser::_normalizeNewLines(void)
{
	std::string normalized;
	normalized.reserve(_buffer.size());
	for (size_t i = 0; i < _buffer.size(); ++i)
	{
		if (_buffer[i] == '\r')
		{
			if (i + 1 < _buffer.size() && _buffer[i + 1] == '\n')
			{
				normalized += "\r\n";
				++i;
			}
			else
				normalized += "\r\n";
		}
		else if (_buffer[i] == '\n')
			normalized += "\r\n";
		else
			normalized += _buffer[i];
	}
	_buffer = std::move(normalized);
}

/**
 *	Parse the internal buffer into the output queue.
 *	Newlines are \r\n as per IRC protocol.
 */
void	RecvParser::_parseBuffer(void)
{
	size_t pos;
	while ((pos = _buffer.find("\r\n")) != std::string::npos)
	{
		std::string line = _buffer.substr(0, pos);
		_buffer.erase(0, pos + 2);
		try
		{
			Message command = _parseMessage(line);
			_output.push(std::make_unique<Message>(std::move(command)));
		}
		catch (std::exception &e)
		{
			std::cerr << "Recv parsing error: " << e.what() << std::endl;
		}
	}
}

Message	RecvParser::_parseMessage(const std::string &msg)
{
	if (msg.size() > 512)
		throw (std::runtime_error("Line is over 512 bytes"));

	Message ret;

	std::string line = msg;
	auto end = line.find_last_not_of("\r\n");
	if (end != std::string::npos)
		line = line.erase(end + 1);
	std::istringstream line_stream(line);
	std::string	token;

	if (line[0] == ':')
	{
		if (!(line_stream >> token))
			throw (std::runtime_error("Malformed prefix"));
		ret.prefix = token.substr(1);
	}

	if (!(line_stream >> ret.command))
		throw (std::runtime_error("Line is missing a command"));

	while (line_stream >> token)
	{
		if (token[0] == ':')
		{
			std::string trailing_param = token.substr(1);
			std::string rest;
			std::getline(line_stream, rest);
			if (!rest.empty() && ret.command != "PRIVMSG")
				trailing_param += " " + rest;
			else if (!rest.empty())
				trailing_param += rest;
			ret.params.push_back(trailing_param);
			break ;
		}
		else
			ret.params.push_back(token);
	}
	return (ret);
}
