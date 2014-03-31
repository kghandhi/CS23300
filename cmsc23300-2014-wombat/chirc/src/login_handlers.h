#ifndef LOGIN_HANDLERS_H
#define LOGIN_HANDLERS_H

int handle_NICK(char* params, char* nick, int socket);

int handle_USER (char* params, char* user, int socket);

#endif
