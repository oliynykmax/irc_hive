#pragma once

#define HOST ":localhost "
#define NICK irc->getClient(fd)->getUser().getNick()
#define USER(X) irc->getClient(X)->getUser()
#define PREFIX irc->getClient(fd)->getUser().createPrefix()
#define MSG ":" + USER(user).getNick() + " " + type \
+ " " + _name + " :" + msg + "\r\n"
#define PARAM msg.params[0]
#define PARAM1 msg.params[1]
#define PARAM2 msg.params[2]
#define PRIVMSG ":" + NICK + " PRIVMSG " + PARAM + " :" + PARAM1
#define PONG "PONG localhost :" + PARAM
#define INVITE PREFIX + " INVITE " + PARAM + " :" + PARAM1
#define KICK PREFIX + " KICK " + PARAM + " " + PARAM1
#define CAP ":localhost CAP * LS :"
#define CAP410 "410 CAP :Unsupported subcommand"
#define CAP461 "461 CAP :Not enough parameters"
#define R315 "315 " + nick + " :End of /WHO list"
#define R318 "318 " + NICK + " :End of WHOIS list"
#define R324 ":localhost 324 " + NICK +	" " + PARAM + " " + ch->modes()
#define R329 ":localhost 329 " + NICK + " " + PARAM + " " + ch->getTime()
#define R331 "331 " + NICK + " " + PARAM + " :No topic is set"
#define R332 "332 " + NICK + " " + PARAM + " :" + topic
#define R341 "341 " + NICK + " " + PARAM + " " + PARAM1 + " :Invitation send "
#define R352 "352 " + PARAM + " " + user.getUser() + " "\
+ user.getHost() + " localhost " + user.getNick() + " H"
#define R353 "353 " + NICK + " @ " + PARAM + " :" + names
#define R366 "366 " + NICK + " " + PARAM + " :End of NAMES list"
#define E401 "401 :No such nick"
#define E403 "403 :No such channel"
#define E403REV2 "403 " + NICK + " " + PARAM + " :No such channel"
#define E409 "409 :No origin specified"
#define E411 "411 :No recipient given"
#define E412 "412 :No text to send"
#define E421 "421 :Unknown command"
#define E422 "422 :You're not on that channel"
#define E431 "431 :No nickname given"
#define E432 "432 " + oldNick + " " + newNick + " :Erroneous nickname"
#define E433 "433 * " + newNick + " :Nickname is already in use"
#define E441 "441 " + NICK + " " + PARAM + " :They aren't on that channel"
#define E442 "442 :You're not on that channel"
#define E443 "443 :User already on channel"
#define E461 "461 :Missing parameters"
#define E462 "462 " + NICK + " : You may not reregister"
#define E464 "464 " + NICK + " :Incorrect password"
#define E471 "471 " + nick + " " + _name + " :Cannot join channel (+l)"
#define E472 "472 :Unknown mode"
#define E473 "473 " + nick + " " + _name + " :Cannot join channel (+i)"
#define E475 "475 " + nick + " " + _name + " :Cannot join channel (+k)"
#define E481 "481 :Permission Denied- You're not an IRC operator"
#define E482 "482 :You're not a channel operator"
#define E502 "502 :Users don't match"
