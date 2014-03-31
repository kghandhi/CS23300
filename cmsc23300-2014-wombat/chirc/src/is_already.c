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
#include "is_already.h"
#include "globals.h"

/*############added function for checking if a user is already on a channel############*/
/*NOTE: You must supply the userlist to check for this function*/
int is_already_chanUser(chanUser* list, int socket) {
  chanUser* search = list;

  while (search != NULL) {
    if (search->sock == socket)
      return 1;
    search = search->next;
  }
 
  return 0;
}

/*############added function for checking if a channel is already on a user's list############*/
/*NOTE: You must supply the channel list to check for this function*/
int is_already_chanName(chanName* list, char* channame) {
  chanName* search = list;

  while (search != NULL) {
    if (!strcmp(search->name,channame))
      return 1;
    search = search->next;
  }
 
  return 0;
}

int is_already_nick(char* nickname) {//in the functions where we call the following functions we lock before calling the function.
  client* search = clients;

  if (strlen(nickname))
    while (search != NULL) {
      if (!strcmp(search->nick,nickname))
        return 1;
      search = search->next;
    }
  return 0;
}

/*From Part B but adding comment to only use for debugging*/
/*as usernames are not unique*/
int is_already_user(char* username) {
  client* search = clients;

  if (strlen(username))
    while (search != NULL) {
      if (!strcmp(search->user,username))
        return 1;
      search = search->next;
    }
 
  return 0;
}

/*From Part B but adding comment to only use for debugging*/
/*as real names are not unique*/
int is_already_real(char* realname) {
  client* search = clients;
  
  if (strlen(realname))
    while (search != NULL) {
      if (!strcmp(search->real,realname))
        return 1;
      search = search->next;
    }
  return 0;
}

/*From Part B but adding comment to only use for debugging*/
/*as hostnames are not unique*/
int is_already_host(char* hostname) {
  client* search = clients;
  
  if (strlen(hostname))
    while (search != NULL) {
      if (!strcmp(search->host,hostname))
        return 1;
      search = search->next;
    }
  return 0;
}

/*############added function for checking if a channel exists############*/
int is_already_chan(char* channame) {
  channel* search = channels;

  if (strlen(channame))
    while (search != NULL) {
      if (!strcmp(search->name,channame))
        return 1;
      search = search->next;
    }
 
  return 0;
}

int is_registered(int socket) {
  client* search = clients;
  
  while(search != NULL) {
    if (search->sock==socket)
      return 1;
    search = search->next;
  }
  return 0;
}
