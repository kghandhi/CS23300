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
#include "away.h"
#include "globals.h"

/*############added function for checking if a user is away############*/
int is_away(int socket) {
  client* search = clients;

  while (search != NULL) {
    if (search->sock == socket)
      return search->away;
    search = search->next;
  }
  return 0;
}

/*############added function for making a user away############*/
void make_away(int socket) {
  client* search = clients;

  while (search != NULL) {
    if (search->sock == socket) {
      search->away = 1;
      return;
    }
    search = search->next;
  }
  return;
}

/*############added function for making a user away############*/
void make_not_away(int socket) {
  client* search = clients;

  while (search != NULL) {
    if (search->sock == socket) {
      search->away = 0;
      return;
    }
    search = search->next;
  }
  return;
}

/*############added function for changing a user's away status############*/
void change_away(int socket) {
  client* search = clients;

  while (search != NULL) {
    if (search->sock == socket) {
      search->away = !search->away;
      return;
    }
    search = search->next;
  }
  return;
}

/*############added function for checking if a user has an away message############*/
int has_awayMsg(int socket) {
  client* search = clients;

  while (search != NULL) {
    if (search->sock == socket)
      return (search->awayMsg != NULL)? 1 : 0;
    search = search->next;
  }
  return 0;
}

/*############added function for getting a user's away message############*/
char* get_awayMsg(int socket) {
  client* search = clients;

  while (search != NULL) {
    if (search->sock == socket)
      return search->awayMsg;
    search = search->next;
  }
  return NULL;
}

/*############added function for changing a user's away message############*/
void change_awayMsg(int socket, char* amsg) {
  client* search = clients;

  while (search != NULL) {
    if (search->sock == socket) {
      if (search->awayMsg != NULL)
	free(search->awayMsg);
      search->awayMsg = strdup(amsg);
      return;
    }
    search = search->next;
  }
  return;
}

/*############added function for removing a user's away message############*/
void remove_awayMsg(int socket) {
  client* search = clients;
  
  while (search != NULL) {
    if (search->sock == socket) {
      if (search->awayMsg != NULL) {
	free(search->awayMsg);
	search->awayMsg = NULL;
      }
      return;
    }
    search = search->next;
  }
  return;
}
