#ifndef SERVERMSG_HANDLERS_H
#define SERVERMSG_HANDLERS_H

int handle_PING(char *placeholder, char *holdplacer, int socket);

int handle_PONG(char *placeholder, char *holdplacer, int socket);

int handle_MOTD(char* placeholder, char* holdplacer, int socket);

int handle_LUSERS(char* placeholder, char* holdplacer, int socket);

int handle_WHOIS (char* args, char* placeholder, int socket);

int handle_NAMES(char* params, char* placeholder, int socket);

int handle_LIST(char* params, char* placeholder, int socket);

int handle_WHO (char* params, char* placeholder, int socket);

#endif
