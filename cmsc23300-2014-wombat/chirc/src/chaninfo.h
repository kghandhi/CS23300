#include "globals.h"

#ifndef CHANINFO_H
#define CHANINFO_H

int is_empty_channel(char* channame);

int is_chan_on_user(char* channame, int socket);

int is_user_on_chan(int socket, char* channame);

int on_one_of_channels(chanName* chanlist, int socket);

int is_member_on_chan(char* member, chanUser* lst);

#endif
