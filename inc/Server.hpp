#pragma once
#include <array>
#include <vector>
#include <string>
#include <future>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include "Client.hpp"
#include <sys/epoll.h>
#include <unordered_map>

constexpr static const std::array<uint32_t, 6> eventTypes{EPOLLIN, EPOLLOUT, EPOLLRDHUP, EPOLLPRI, EPOLLERR, EPOLLRDHUP};

class Server {
	private:
		std::unordered_map<int, class Client> _clients{};
		std::vector<epoll_event> _events{};
		const int _fd;
		const int _max_events = 100;
		std::string _password;
		void _reloadHandler(Client &client) const;
	public:
		Server(std::string passwd = "");
		virtual ~Server();
		void addClient(Client client);
		void removeClient(const int fd);
		void registerHandler(const int fd, uint32_t eventType, std::function<void(int)> handler);
		void addOwnSocket(int sockfd);
		/*
		 * @brief Compare password to servers, or without argument if one is set
		* @param password the user provided PASS, default ""
		* @return true if match or empty, false if neither
		 */
		bool checkPassword(std::string password = "") const;
		void poll(int tout = -1);
		const std::unordered_map<int, class Client>& getClients() const;
		Client& getClient(int fd);
		int getServerFd() const;

};
