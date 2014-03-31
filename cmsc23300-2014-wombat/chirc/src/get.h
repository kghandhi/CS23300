#include <pthread.h>
#include "globals.h"

#ifndef GET_H
#define GET_H

chanUser* get_users_on_channel(char* channame);

chanName* get_channels_on_user(int socket);

char* get_user_from_nick(char* nickname);

char* get_user_from_sock(int socket);

char* get_nick_from_sock(int socket);

int get_sock_from_nick(char* nickname);

char* get_host_from_nick(char* nickname);

char* get_host_from_sock(int socket);

char* get_real_from_nick(char* nickname);

char* get_real_from_sock(int socket);

pthread_mutex_t get_lock_from_chan(char *chan);

pthread_mutex_t get_lock_from_nick(char* nickname);

pthread_mutex_t get_lock_from_sock(int socket);

#endif
