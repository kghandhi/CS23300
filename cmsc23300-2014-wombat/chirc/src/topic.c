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
#include "topic.h"
#include "globals.h"

/*############added function for checking if a channel has a topic############*/
int has_topic(char* channame) {
  channel* search = channels;
  
  while (search != NULL) {
    if (!strcmp(search->name, channame)) {
      if (search->topic != NULL && strlen(search->topic))
	return 1;
      return 0;
    }
    search = search->next;
  }
  return 0;
}

/*############added function for getting a channel's topic############*/
char* get_topic(char* channame) {
  channel* search = channels;

  while (search != NULL) {
    if (!strcmp(search->name, channame))
      return search->topic;
    search = search->next;
  }
  return NULL;
}

/*############added function for changing a channel's topic############*/
void change_topic(char* channame, char* topic) {
  channel* search = channels;

  while (search != NULL) {
    if (!strcmp(search->name, channame)) {
      if (search->topic != NULL)
	free(search->topic);
      search->topic = strdup(topic);
      return;
    }
    search = search->next;
  }
  return;
}

/*############added function for removing a channel's topic############*/
void remove_topic(char* channame) {
  channel* search = channels;

  while (search != NULL) {
    if (!strcmp(search->name, channame)) {
      if (search->topic != NULL) {
	free(search->topic);
	search->topic = NULL;
      }
      return;
    }
    search = search->next;
  }
  return;
}
