#ifndef MSG_HANDLERS_H
#define MSG_HANDLERS_H

int handle_PRIVMSG(char *args, char *placeholder, int socket);

int handle_NOTICE(char *args, char* placeholder, int socket);

#endif
