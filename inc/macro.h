#pragma once

#define HOST ":localhost "
#define NICK irc->getClient(fd).getUser()->getNick()
#define USER(X) irc->getClient(X).getUser()
#define PREFIX irc->getClient(fd).getUser()->createPrefix()
#define PARAM msg.params[0]
#define PARAM1 msg.params[1]
#define PARAM2 msg.params[2]
#define R324 ":localhost 324 " + NICK +	" " + PARAM + " " + ch->modes()
#define R329 ":localhost 329 " + NICK + " " + PARAM + " " + ch->getTime()
#define R331 "331 " + NICK + " " + PARAM + " :No topic is set"
#define R332 "332 " + NICK + " " + PARAM + " :" + topic
#define R341 "341 :Invitation send"
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
#define E441 "441 :They aren't on that channel"
#define E442 "442 :You're not on that channel"
#define E443 "443 :User already on channel"
#define E461 "461 :Missing parameters"
#define E462 "462 " + NICK + " : You may not reregister"
#define E471 "471 " + nick + " " + _name + " :Cannot join channel (+l)"
#define E472 "472 :Unknown mode"
#define E473 "473 " + nick + " " + _name + " :Cannot join channel (+i)"
#define E475 "475 " + nick + " " + _name + " :Cannot join channel (+k)"
#define E481 "481 :Permission Denied- You're not an IRC operator"
#define E482 "482 :You're not a channel operator"
#define E502 "502 :Users don't match"
