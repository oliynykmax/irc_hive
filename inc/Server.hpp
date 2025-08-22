#pragma once
#include <array>
#include <vector>
#include <string>
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
		const int _max_events = 10;
		std::string _password;
		void _reloadHandler(Client &client) const;
	public:
		Server(std::string passwd = "");
		virtual ~Server();
		void addClient(const Client client);
		void removeClient(const Client client);
		void registerHandler(const int fd, uint32_t eventType, std::function<void(int)> handler);
		void addOwnSocket(int sockfd);

		void poll(int tout = -1);
		const std::unordered_map<int, class Client>& getClients() const;
		int getServerFd() const;

};
