#ifndef CHANNEL_HANDLERS_H
#define CHANNEL_HANDLERS_H

int handle_JOIN(char* params, char* placeholder, int socket);

int handle_PART(char* params, char* placeholder, int socket);

int handle_TOPIC(char* params, char* placeholder, int socket);

#endif
