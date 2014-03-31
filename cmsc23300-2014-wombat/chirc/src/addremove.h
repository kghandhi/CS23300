#ifndef ADDREMOVE_H
#define ADDREMOVE_H

void add_client(char* nickname, char* username, char* hostname, char* realname, int socket);

void remove_client(int socket);

void add_channel(char* name);

void remove_channel(char* channame);

void add_chanUser(char* channame, char* nickname, int socket);

void remove_chanUser(char* channame, int socket);

void add_chanName(int socket, char* channame);

void remove_chanName(int socket, char* channame);

void sock_is_gone(int socket);

#endif
