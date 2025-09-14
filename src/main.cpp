#include "Server.hpp"
#include "Handler.hpp"
#include <iostream>
#include <stdexcept>
#include <csignal>

using namespace std;
Server *irc;

namespace {
	volatile sig_atomic_t gSigStatus = 0;
}

int main(int argc, char *argv[]) {
	if (argc != 2 && argc != 3) {
		cerr << "Usage ./ircserv [port] <password>" << endl;
		return 1;
	}

	signal(SIGINT, [](int) { gSigStatus = 1; });
	signal(SIGQUIT, [](int) { gSigStatus = 1; });

	try {
		if (argc == 3)
			irc = new Server(string(argv[1]), string(argv[2]));
		else
			irc = new Server(string(argv[1]));
	} catch (runtime_error &err) {
		cerr << err.what() << endl;
		return 1;
	} catch (bad_alloc &a) {
		cerr << "Memory allocation failed" << endl;
		return 1;
	} catch (...) {
		cerr << "Bad port number" << endl;
		return 1;
	}

	while (not gSigStatus) {
		try {
			irc->poll();
		} catch (runtime_error &err) {
			cerr << err.what() << endl;
		}
	}
	delete irc;
}
