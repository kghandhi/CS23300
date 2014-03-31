#ifndef HANDLERS_H
#define HANDLERS_H

int handle_NICK(char* params, char* nick, int socket);

int handle_USER (char* params, char* user, int socket);

int handle_PRIVMSG(char *args, char *placeholder, int socket);

int handle_NOTICE(char *args, char* placeholder, int socket);

int handle_PING(char *placeholder, char *holdplacer, int socket);

int handle_PONG(char *placeholder, char *holdplacer, int socket);

int handle_MOTD(char* placeholder, char* holdplacer, int socket);

int handle_LUSERS(char* placeholder, char* holdplacer, int socket);

int handle_WHOIS (char* args, char* placeholder, int socket);

int handle_QUIT(char* params, char* placeholder, int socket);

int handle_NAMES(char* params, char* placeholder, int socket);

int handle_JOIN(char* params, char* placeholder, int socket);

int handle_PART(char* params, char* placeholder, int socket);

int handle_TOPIC(char* params, char* placeholder, int socket);

int handle_OPER(char* args, char* placeholder, int socket);

int handle_MODE(char* args, char* placeholder, int socket);

int handle_AWAY(char* params, char* placeholder, int socket);

int handle_LIST(char* params, char* placeholder, int socket);

int handle_WHO (char* params, char* placeholder, int socket);

#endif
