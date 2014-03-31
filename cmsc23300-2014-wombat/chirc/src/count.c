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
#include "count.h"
#include "globals.h"

/*############added function for counting all users on channel############*/
int count_users_on_channel(char* channame) {
  channel* search = channels;
  int count = 0;

  while (search != NULL) {
    if (!strcmp(search->name, channame)) {
      chanUser* search2 = search->users;
      while (search2 != NULL) {
	count++;
	search2 = search2->next;
      }
      return count;
    }
    search = search->next;
  }
  return count;
}

/*############added function for counting all channels on user############*/
int count_channels_on_user(int socket) {
  client* search = clients;
  int count = 0;
  
  while (search != NULL) {
    if (search->sock == socket) {
      chanName* search2 = search->chans;
      while (search2 != NULL) {
	count++;
	search2 = search2->next;
      }
      return count;
    }
    search = search->next;
  }
  return count;
}

/*############added function for counting number of channels############*/
int count_channels() {
  channel* search = channels;
  int count = 0;

  while (search != NULL) {
    count++;
    search = search->next;
  }
  return count;
}

/*############added function for counting number of operators############*/
int count_operators() {
  client* search = clients;
  int count = 0;
  
  while (search != NULL) {
    if (search->oper)
      count++;
    search = search->next;
  }
  return count;
}
