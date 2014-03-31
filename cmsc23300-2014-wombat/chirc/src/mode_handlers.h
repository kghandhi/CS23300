#ifndef MODE_HANDLERS_H
#define MODE_HANDLERS_H

int handle_OPER(char* args, char* placeholder, int socket);

int handle_MODE(char* args, char* placeholder, int socket);

int handle_AWAY(char* params, char* placeholder, int socket);

#endif
