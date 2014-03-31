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
#include "modes.h"
#include "get.h"
#include "globals.h"

char* get_modes(char* chan){
 
  char* ret = (char*)malloc(sizeof(char)*5*MODE_LEN);
  ret[0] = '\0';
  channel* search = channels;
  while (search != NULL) {
    if (!strcmp(search->name, chan)){
      if (search->modMode && search->topMode){
	sprintf(ret, "tm");
      } else {
	if (search->modMode){
	  sprintf(ret, "m");
	} 
	if (search->topMode){
	  sprintf(ret, "t");
	}
      }
      break;
    } 
    search = search->next;
  }
  return ret;
}


int deopp(int socket){
  client* search = clients;

  while (search != NULL){
    if (search->sock == socket){
 
      search->oper = 0;
      return 1;
    }
    search = search->next;
  }
  return 0;
}

int make_opp(int socket){
  client* search = clients;

  while (search != NULL){
    if (search->sock == socket){
      search->oper = 1;
      return 1;
    }
    search = search->next;
  }
  return 0;
}


int decopp(int socket, char* chan){
  chanName* search = get_channels_on_user(socket);

  while(search != NULL) {
    if (!strcmp(search->name, chan)) {
      search->coper = 0;
      break;   
    }
    search = search->next;
  }

  chanUser* search2 = get_users_on_channel(chan);

  while(search2 != NULL) {
    if(search2->sock == socket) {
      search2->coper = 0;
      break;
    }
    search2 = search2->next;
  }
  return 1;
}

int make_copp(int socket, char* chan){
  chanName* search = get_channels_on_user(socket);

  while(search != NULL) {
    if (!strcmp(search->name, chan)){
      search->coper = 1;
      break;
    }
    search = search->next;
  }

  chanUser* search2 = get_users_on_channel(chan);
  
  while(search2 != NULL) {
    if(search2->sock == socket) {
      search2->coper = 1;
      break;
    }
    search2 = search2->next;
  }
  return 1;
}
 
int  make_voice(int socket, char* chan){
  chanName* search = get_channels_on_user(socket);

  while(search != NULL) {
    if (!strcmp(search->name, chan)){
      search->voice = 1;
      break;
    }
    search = search->next;
  }

  chanUser* search2 = get_users_on_channel(chan);
  
  while(search2 != NULL) {
    if(search2->sock == socket) {
      search2->voice = 1;
      break;
    }
    search2 = search2->next;
  }
  return 1;
}


int devoice(int socket, char* chan){
  chanName* search = get_channels_on_user(socket);

  while(search != NULL) {
    if (!strcmp(search->name, chan)){
      search->voice = 0;
      break;
    }
    search = search->next;
  }

  chanUser* search2 = get_users_on_channel(chan);
  
  while(search2 != NULL) {
    if(search2->sock == socket) {
      search2->voice = 0;
      break;
    }
    search2 = search2->next;
  }
  return 1;
}

int get_voice_from_sock(int socket, char* chan) {
  chanName *search = get_channels_on_user(socket);

  while (search != NULL) {
    if (!strcmp(chan, search->name)) {
      return search->voice;
    }
    search = search->next;
  }
  return 0;
}

int get_modMode(char *chan) {
  pthread_mutex_t chlock = get_lock_from_chan(chan);
  pthread_mutex_lock(&chlock);
  channel *search = channels;
  pthread_mutex_unlock(&chlock);

  while (search != NULL) {
    if (!strcmp(search->name, chan)) {
      return search->modMode;
    }
    search = search->next;
  }
  return 0;
}

int get_topMode(char *chan) {
  channel *search = channels;
      
  while (search != NULL) {
    if (!strcmp(search->name, chan)) {
      return search->topMode;
    }
    search = search->next;
  }
  return 0;
}

int change_chan_mode(char* chan, int mode, int on_off){
  pthread_mutex_lock(&lock2);
  channel* search = channels;
  
  while (search != NULL){
    if (!strcmp(chan, search->name)){
      pthread_mutex_t chlock = get_lock_from_chan(search->name);
      pthread_mutex_unlock(&lock2);
      pthread_mutex_lock(&chlock);
      if (mode == 1){
	search->modMode = on_off;
      }
      if (mode == 2){
	search->topMode = on_off;
      }
      pthread_mutex_unlock(&chlock);
      return 1;
    }
    search = search->next;
  }
  pthread_mutex_unlock(&lock2);
  return 0;
}

int get_oper_from_sock(int socket) {
  client *search = clients;
  
  while(search != NULL){
    if (search->sock == socket){
      return search->oper;
    }
    search = search->next;
  }
  return 0;
}

int get_coper_from_sock(int socket, char* channel) {
  chanName *search = get_channels_on_user(socket);

  while (search != NULL){
    if (!strcmp(channel, search->name)){
      return search->coper;
    }
    search = search->next;
  }
  return 0;
}
