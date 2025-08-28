#pragma once
#include <functional>
#include <cstdint>
#include "User.hpp"
class User;
/*
 * @class Client
 * @brief Handles events on files registered to epoll
 * @param IN function pointer for reading input
 * @param OUT function pointer for writing output
 * @param RDHUP function pointer for when sending end closes
 * @param PRI function pointer for an importan message
 * @param ERR function pointer for handling errors
 * @param HUP function pointer for disconnects
 * @param _fd file descriptor of a network socket
 * @param _initialized state of epoll registration
 * @param _self instance of a User class.
 */
class Client {
	private:
		std::function<void(int)> IN = 		nullptr;
		std::function<void(int)> OUT = 		nullptr;
		std::function<void(int)> RDHUP =  	nullptr;
		std::function<void(int)> PRI = 		nullptr;
		std::function<void(int)> ERR = 		nullptr;
		std::function<void(int)> HUP =  	nullptr;
		User _self;

	public:
		explicit Client(int fd);
		const int _fd;
		bool _initialized = false;
		bool handler(uint32_t eventType) const;
		void setHandler(uint32_t eventType, std::function<void(int)> handler);
		std::function<void(int)>& getHandler(uint32_t eventType);
		User& getUser(void);
};
