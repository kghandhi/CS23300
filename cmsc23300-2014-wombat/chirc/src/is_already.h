#include "globals.h"

#ifndef IS_ALREADY_H
#define IS_ALREADY_H

int is_already_chanUser(chanUser* list, int socket);

int is_already_chanName(chanName* list, char* channame);

int is_already_nick(char* nickname);

int is_already_user(char* username);

int is_already_real(char* realname);

int is_already_host(char* hostname);

int is_already_chan(char* channame);

int is_registered(int socket);

#endif
