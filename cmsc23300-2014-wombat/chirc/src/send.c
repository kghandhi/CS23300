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
#include "send.h"
#include "get.h"
#include "globals.h"

/*############added function for sending messages to all users on channel############*/
/*NOTE: this is distinct from privmsg and notice*/
void send_to_channel(char* channame, char* msg) {
  channel* search = channels;
  pthread_mutex_t clock;

  if (msg  != NULL) {
    while (search != NULL) {
      if (!strcmp(search->name, channame)) {
	chanUser* search2 = search->users;
	while (search2 != NULL) {
          clock = get_lock_from_sock(search2->sock);
          pthread_mutex_lock(&clock);
	  if(send(search2->sock, msg, strlen(msg), 0) <= 0) {
            pthread_mutex_unlock(&clock);
	    sock_is_gone(search2->sock);
	    perror("Socket send() failed");
	    close(search2->sock);
	  }
          pthread_mutex_unlock(&clock);
	  search2 = search2->next;
	}
	return;
      }
      search = search->next;
    }
  }
  return;
}

/*############added function for sending messages to all other users on channel############*/
/*NOTE: this is distinct from privmsg and notice*/
void send_to_rest_of_channel(char* channame, char* msg, int sendSock) {
  channel* search = channels;
  pthread_mutex_t clock;

  if (msg != NULL) {
    while (search != NULL) {
      if (!strcmp(search->name, channame)) {
	chanUser* search2 = search->users;
	while (search2 != NULL) {
	  if (search2->sock != sendSock) {
	    clock = get_lock_from_sock(search2->sock);
            pthread_mutex_lock(&clock);
	    if (send(search2->sock, msg, strlen(msg), 0) <= 0) {
              pthread_mutex_unlock(&clock);
	      sock_is_gone(search2->sock);
	      perror("Socket send() failed");
	      close(search2->sock);
	    }
            pthread_mutex_unlock(&clock);
          }
	  search2 = search2->next;
	}
	return;
      }
      search = search->next;
    }
  }
  return;
}

/*############added function for sending two messages to all other users on channel############*/
/*NOTE: this is distinct from privmsg and notice*/
void send2_to_rest_of_channel(char* channame, char* msg1, char* msg2, int sendSock) {
  channel* search = channels;
  pthread_mutex_t clock;

  if ((msg1 != NULL) && (msg2 != NULL)) {
    while (search != NULL) {
      if (!strcmp(search->name, channame)) {
	chanUser* search2 = search->users;
	while (search2 != NULL) {
	  if (search2->sock != sendSock) {
	    clock = get_lock_from_sock(search2->sock);
            pthread_mutex_lock(&clock);
	    if (send(search2->sock, msg1, strlen(msg1), 0) <= 0) {
              pthread_mutex_unlock(&clock);
	      sock_is_gone(search2->sock);
	      perror("Socket send() failed");
	      close(search2->sock);
	    }
	    if (send(search2->sock, msg2, strlen(msg2), 0) <= 0) {
              pthread_mutex_unlock(&clock);
	      sock_is_gone(search2->sock);
	      perror("Socket send() failed");
	      close(search2->sock);
	    }
            pthread_mutex_unlock(&clock);
	  }
	  search2 = search2->next;
	}
	return;
      }
      search = search->next;
    }
  }
  return;
}
