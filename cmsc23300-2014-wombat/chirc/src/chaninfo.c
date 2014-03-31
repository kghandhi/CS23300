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
#include "chaninfo.h"
#include "globals.h"

/*############added function for checking if a channel is empty############*/
/*NOTE: should only be called if we know the channel exists*/
/*To that point, returns 0 if channel does not exist as a channel can't be empt\y if it isn't a channel*/
int is_empty_channel(char* channame) {
  channel* search = channels;

  while (search != NULL) {
    if (!strcmp(search->name, channame))
      return (search->users == NULL)? 1 : 0;
    search = search->next;
  }
  return 0;
}



/*############added function for checking if a channel is in a client struct's list############*/
int is_chan_on_user(char* channame, int socket) {
  client* search = clients;

  if (strlen(channame))
    while (search != NULL) {
      if (search->sock == socket) {
	chanName* search2 = search->chans;
	while(search2 != NULL) {
	  if (!strcmp(search2->name, channame))
	    return 1;
	  search2 = search2->next;
	}
	return 0;
      }
      search = search->next;
    }
  return 0;
}

/*############added function for checking if a user is in a channel struct's list############*/
int is_user_on_chan(int socket, char* channame) {
  channel* search = channels;

  if (strlen(channame))
    while (search != NULL) {
      if (!strcmp(search->name, channame)) {
	chanUser* search2 = search->users;
	while(search2 != NULL) {
	  if (search2->sock == socket)
	    return 1;
	  search2 = search2->next;
	}
	return 0;
      }
      search = search->next;
    }
  return 0;
}

/*############added function for checking if a user is on any of a list of channels############*/
int on_one_of_channels(chanName* chanlist, int socket) {
  chanName* search = chanlist;
  
  while(search != NULL) {
    if (is_user_on_chan(socket, search->name))
      return 1;
    search = search->next;
  }
  return 0;
}

int is_member_on_chan(char* member, chanUser* lst){
  chanUser* search = lst;
  while (search != NULL) {
    if (!strcmp(search->nick, member)) {
      return 1;
    }
    search = search->next;
  }
  return 0;
}
