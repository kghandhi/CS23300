#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include "change.h"
#include "globals.h"

void change_nick_from_sock(int socket, char* nickname) {
  client* search = clients;
  
  while(search != NULL) {
    if (search->sock==socket) {
      if (search->nick != NULL)
free(search->nick);
      search->nick = strdup(nickname);
      return;
    }
    search = search->next;
  }
}

void change_user_from_nick(char* nickname, char* username) {
  client* search = clients;
  
  while(search != NULL) {
    if (!strcmp(search->nick, nickname)) {
      if (search->user != NULL)
free(search->user);
      search->user = strdup(username);
      return;
    }
    search = search->next;
  }
}

void change_user_from_sock(int socket, char* username) {
  client* search = clients;
  
  while(search != NULL) {
    if (search->sock==socket) {
      if (search->user != NULL)
free(search->user);
      search->user = strdup(username);
      return;
    }
    search = search->next;
  }
}

void change_real_from_nick(char* nickname, char* realname) {
  client* search = clients;

  while(search != NULL) {
    if (!strcmp(search->nick, nickname)) {
      if (search->real != NULL)
free(search->real);
      search->real = strdup(realname);
      return;
    }
    search = search->next;
  }
}

void change_real_from_sock(int socket, char* realname) {
  client* search = clients;

  while(search != NULL) {
    if (search->sock==socket) {
      if (search->real != NULL)
	free(search->real);
      search->real = strdup(realname);
      return;
    }
    search = search->next;
  }
}
