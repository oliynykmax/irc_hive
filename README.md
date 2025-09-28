# irc_hive

Welcome to **irc_hive**, our C++ implementation of an IRC (Internet Relay Chat) system. IRC is a protocol for real-time text messaging over the internet, like chat rooms from the early web days.

## What We Did

We built a full IRC server in C++20 that manages clients, channels, and commands (JOIN, PART, PRIVMSG, etc.). Plus, a bot that connects to servers, joins channels, responds to commands, and filters bad words. The server works with **irssi** as an irc client.

## SERVER FEATURES PLEASE ADD ....

### Bot Features

The bot shines with its smart word-filtering system – only channel operators can add/remove banned words, and it auto-kicks offenders to keep chats clean.

We're proud of the robust reconnection logic with exponential backoff that handles network hiccups gracefully, and the extensible command system that lets you easily add new features.

It also accepts invites to join new channels automatically.

Available commands:
- `!ping` - Replies with "pong!"
- `!help` - Lists available commands
- `!filter <word> ...` - Adds words to the channel filter (operators only)
- `!unfilter <word> ...` - Removes words from the channel filter (operators only)
- `!listfilter` - Lists filtered words in the channel

It's a powerful automation tool for IRC, especially with irssi integration.

## Shared Responsibilities

- [Joonas](https://github.com/jotuel): Server
- [Ville](https://github.com/v-kuu): Server
- [Max](https://github.com/oliynykmax): Bot

## Getting Started

Build: `make all` `make bot`

Run server: `./ircserv <port> [password]`

Run bot: `./ircbot -s <server> -p <port> -c <channels>`
