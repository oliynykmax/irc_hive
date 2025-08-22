#pragma once
#include <functional>
#include <cstdint>

class Client {
	private:
		std::function<void(int)> IN = nullptr;
		std::function<void(int)> OUT = nullptr;
		std::function<void(int)> RDHUP =  nullptr;
		std::function<void(int)> PRI = nullptr;
		std::function<void(int)> ERR = nullptr;
		std::function<void(int)> HUP =  nullptr;

	public:
		explicit Client(int fd);
		const int _fd;
		bool isInitialized = false;
		bool handler(uint32_t eventType) const;
		void setHandler(uint32_t eventType, std::function<void(int)> handler);
		std::function<void(int)>& getHandler(uint32_t eventType);
};
