#pragma once
#include <vector>
#include <optional>
#include <string>

struct	Message
{
	std::optional<std::string>	prefix;
	std::string					command;
	std::vector<std::string>	params;
};
