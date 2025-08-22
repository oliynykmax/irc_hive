#include "Operator.hpp"

void Operator::kick(string username) {
	(void)username;
}

void Operator::invite(string username) {
	(void)username;
}

void topic(Channel *chan) {
	(void)chan;
}

void mode(enum mode a, string option) {
	switch(a) {
		default:
			(void) option;
	}
}
