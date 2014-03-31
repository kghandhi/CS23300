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
#include "get.h"
#include "globals.h"

/*############added function for getting a list of all users on channel############*/
chanUser* get_users_on_channel(char* channame) {
  channel* search = channels;
  
  while (search != NULL) {
    if (!strcmp(search->name, channame))
      return search->users;
    search = search->next;
  }
  return NULL;
}

/*############added function for getting a list of all channels on user############*/
chanName* get_channels_on_user(int socket) {
  client* search = clients;

  while (search != NULL) {
    if (search->sock == socket)
      return search->chans;
    search = search->next;
  }
  return NULL;
}

char* get_user_from_nick(char* nickname) {
  client* search = clients;

  while(search != NULL) {
    if (!strcmp(search->nick, nickname))
      return search->user;
    search = search->next;
  }
  return NULL;
}

char* get_user_from_sock(int socket) {
  client* search = clients;

  while(search != NULL) {
    if (search->sock==socket)
      return search->user;
    search = search->next;
  }
  return NULL;
}

char* get_nick_from_sock(int socket) {
  client* search = clients;

  while(search != NULL) {
    if (search->sock==socket)
      return search->nick;
    search = search->next;
  }
  return NULL;
}

int get_sock_from_nick(char* nickname) {
  client* search = clients;

  while(search != NULL) {
    if (!strcmp(search->nick,nickname))
      return search->sock;
    search = search->next;
  }
  return -1;
}

char* get_host_from_nick(char* nickname) {
  client* search = clients;

  while(search != NULL) {
    if (!strcmp(search->nick, nickname))
      return search->host;
    search = search->next;
  }
  return NULL;
}

char* get_host_from_sock(int socket) {
  client* search = clients;

  while(search != NULL) {
    if (search->sock==socket)
      return search->host;
    search = search->next;
  }
  return NULL;
}

char* get_real_from_nick(char* nickname) {
  client* search = clients;

  while(search != NULL) {
    if (!strcmp(search->nick, nickname))
      return search->real;
    search = search->next;
  }
  return NULL;
}

char* get_real_from_sock(int socket) {
  client* search = clients;

  while(search != NULL) {
    if (search->sock==socket)
      return search->real;
    search = search->next;
  }
  return NULL;
}

pthread_mutex_t get_lock_from_chan(char *chan){
  pthread_mutex_lock(&lock);
  channel* search = channels;
  pthread_mutex_unlock(&lock);

  while(search != NULL) {
    if (!strcmp(chan, search->name)) {
      return search->lock;
    }
    search = search->next;
  }
  pthread_mutex_t bad;
  return bad;
}

pthread_mutex_t get_lock_from_nick(char* nickname) {
  client* search = clients;

  while(search != NULL) {
    if (!strcmp(search->nick, nickname))
      return search->lock;
    search = search->next;
  }
  //this should never get called, these functions
  //should always check the user they are looking
  //for exists before asking for their lock
  fprintf(stderr,"WARNING: get_lock_from_nick did not find nick in list\n");
  pthread_mutex_t bad = PTHREAD_MUTEX_INITIALIZER;
  return bad;
}

pthread_mutex_t get_lock_from_sock(int socket) {
  client* search = clients;

  while(search != NULL) {
    if (search->sock==socket)
      return search->lock;
    search = search->next;
  }
  //see above, if this code is run then something
  //has gone wrong. this is only here because the
  //function requires a return statement here for
  //the code to compile
  fprintf(stderr,"WARNING: get_lock_from_sock did not find socket in list\n");
  pthread_mutex_t bad = PTHREAD_MUTEX_INITIALIZER;
  return bad;
}
