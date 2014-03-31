#ifndef AWAY_H
#define AWAY_H

int is_away(int socket);

void make_away(int socket);

void make_not_away(int socket);

void change_away(int socket);

int has_awayMsg(int socket);

char* get_awayMsg(int socket);

void change_awayMsg(int socket, char* amsg);

void remove_awayMsg(int socket);

#endif
