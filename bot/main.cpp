#include "Bot.hpp"
#include <iostream>
#include <cstdlib>

static void usage() {
    std::cerr << "Usage: ./ircbot [options]\n"
              << "  -s HOST      IRC server (default: localhost)\n"
              << "  -p PORT      IRC port (default: 6667)\n"
              << "  -n NICK      Nickname (default: ircbot)\n"
              << "  -c CHS       Comma-separated channels, e.g. \"#test,#bots\"\n"
              << "  --pass PASS  Server password\n";
}

int main(int argc, char **argv) {
    Bot::Config cfg;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](std::string &out) {
            if (i + 1 >= argc) { usage(); std::exit(2); }
            out = argv[++i];
        };
        if (a == "-s") need(cfg.host);
        else if (a == "-p") need(cfg.port);
        else if (a == "-n") need(cfg.nick);
        else if (a == "-c") {
            std::string chs; need(chs);
            size_t start = 0;
            while (true) {
                size_t pos = chs.find(',', start);
                std::string token = (pos == std::string::npos) ? chs.substr(start) : chs.substr(start, pos - start);
                if (!token.empty()) cfg.channels.push_back(token);
                if (pos == std::string::npos) break;
                start = pos + 1;
            }
        } else if (a == "--pass") need(cfg.pass);
        else { std::cerr << "Unknown arg: " << a << "\n"; usage(); std::exit(2); }
    }

    Bot bot(cfg);
    bot.run();
    return 0;
}
