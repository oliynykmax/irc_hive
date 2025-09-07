#pragma once
#include <functional>
#include <cstdint>
#include <stdexcept>
#include <sys/epoll.h>
#include <fcntl.h>
#include <memory>
#include "User.hpp"
class User;
/*
 * @class Client
 * @brief Handles events on files registered to epoll
 * @param _IN function pointer for reading input
 * @param _RDHUP function pointer for when sending end closes
 * @param _HUP function pointer for disconnects
 * @param _fd file descriptor of a network socket
 * @param _initialized state of epoll registration
 * @param _self instance of a User class.
 */
class Client {
	private:
		std::function<void(int)> _IN	=	nullptr;
		std::function<void(int)> _RDHUP	=  	nullptr;
		std::function<void(int)> _HUP	= 	nullptr;
		User* _self;
		bool _authenticated = false;
		bool _registered = false;

	public:
		explicit Client(int fd);
		~Client();
		Client(const Client&);
		Client& operator=(const Client&);
		const int _fd;
		bool _initialized = false;
		bool handler(uint32_t eventType) const;
		void setHandler(uint32_t eventType, std::function<void(int)> handler);
		std::function<void(int)>& getHandler(uint32_t eventType);
		User* getUser(void);
		void authenticate(void);
		bool isAuthenticated(void) const;
		bool& accessRegistered(void);
};
