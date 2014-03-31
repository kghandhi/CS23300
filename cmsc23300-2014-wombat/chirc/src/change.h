#ifndef CHANGE_H
#define CHANGE_H

void change_nick_from_sock(int socket, char* nickname);

void change_user_from_nick(char* nickname, char* username);

void change_user_from_sock(int socket, char* username);

void change_real_from_nick(char* nickname, char* realname);

void change_real_from_sock(int socket, char* realname);

#endif
