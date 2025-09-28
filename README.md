# irc_hive

Welcome to **irc_hive**, our C++ implementation of an Internet Relay Chat system. IRC is a protocol for real-time text messaging over the internet, like chat rooms from the early web days.

## What We Did

We built an IRC server in C++20 that manages clients, channels, and commands (JOIN, PART, PRIVMSG, etc.). Plus, a bot that connects to servers, joins channels, responds to commands, and kicks users for writing forbidden words. The server works with **irssi** as an IRC client.

Available commands:
- `!ping` - Replies with "pong!"
- `!help` - Lists available commands
- `!filter <word> ...` - Adds words to the channel filter (operators only)
- `!unfilter <word> ...` - Removes words from the channel filter (operators only)
- `!listfilter` - Lists filtered words in the channel

## Shared Responsibilities

- [Joonas](https://github.com/jotuel): Server
- [Ville](https://github.com/v-kuu): Server
- [Max](https://github.com/oliynykmax): Bot

## Getting Started

Build: `make all` `make bot`

Run server: `./ircserv <port> [password]`

Run bot: `./ircbot -s <server> -p <port> -c <channels>`
