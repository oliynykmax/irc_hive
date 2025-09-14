#pragma once
#include <map>
#include <array>
#include <ctime>
#include <vector>
#include <string>
#include <future>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include "Client.hpp"
#include "Handler.hpp"
#include "Channel.hpp"
#include <sys/epoll.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unordered_map>

constexpr static const std::array<uint32_t, 3> eventTypes{EPOLLIN, EPOLLHUP, EPOLLRDHUP};

class Server {
	private:
		std::map<std::string, class Channel> _channels{};
		std::unordered_map<int, class Client> _clients{};
		std::vector<epoll_event> _events{};
		std::time_t _startTime;
		const int _fd;
		const int _sock;
		std::string::size_type _checker;
		const int _port;
		const int _max_events = 100;
		std::string _password;
		void _reloadHandler(Client &client) const;
		void _addOwnSocket(int sockfd);
	public:
		Server(std::string port = "6667", std::string passwd = "");
		virtual ~Server();
		void addClient(int fd);
		/*
		* @brief Constructs a Channel instance and tries to add it to map.
		* @param name string of the #channel to JOIN
		* @return Reference either to already existing Channel with the given name
		* or newly created one.
		*/
		Channel& addChannel(std::string name);
		void removeChannel(std::string name);
		bool channelExists(std::string name);
		Channel* findChannel(std::string name);
		void removeClient(const int fd);
		void registerHandler(const int fd, uint32_t eventType, std::function<void(int)> handler);
		/*
		* @brief Converts seconds the epoch from Server creation to calendar time
		* @return string of localtime
		*/
		std::string getTime(void) const;
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
