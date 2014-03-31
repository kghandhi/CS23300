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
#include "login_handlers.h"
#include "modes.h"
#include "away.h"
#include "topic.h"
#include "get.h"
#include "globals.h"
#include "addremove.h"
#include "is_already.h"
#include "send.h"
#include "count.h"
#include "chaninfo.h"

int handle_NICK(char* params, char* nick, int socket) {
  int i;
  char* test = (char*)malloc(MAX_NICK_LEN+1);
  int change2Client = 0;
  int lenUser;
  char* changeNickMsg = (char*)malloc(MAX_MSG_LEN+1);

  pthread_mutex_lock(&lock);
  char* user = get_user_from_sock(socket);
 
  if (is_already_nick(nick)) {
    change2Client = 1;
    lenUser = strlen(user);
  }

  for (i = 0; params[i] != '\0' && params[i] != ' ' && i < MAX_NICK_LEN; i++)
    test[i] = params[i];
  test[i] = '\0';

  if(is_already_nick(test)) {
    char* errorMsg = (char*)malloc(MAX_MSG_LEN+1);
    sprintf(errorMsg, ":%s 433 * %s :Nickname is already in use\r\n",
            serverHost,test);
    if (send(socket, errorMsg, strlen(errorMsg), 0) <= 0) {
      pthread_mutex_unlock(&lock);
      sock_is_gone(socket);
      free(test);
      free(changeNickMsg);
      free(errorMsg);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    pthread_mutex_unlock(&lock);
    free(test);
    free(changeNickMsg);
    free(errorMsg);
    return 0;
  }
  
  if (change2Client && lenUser) {
    char* host = get_host_from_sock(socket);
    sprintf(changeNickMsg, ":%s!%s@%s NICK :%s\r\n", nick, user, host, test);
  }
  for (i = 0; test[i]!='\0'; i++)
    nick[i] = test[i];
  nick[i] = '\0';
  if(change2Client) {
    change_nick_from_sock(socket, nick);
    if (lenUser) {
      if (send(socket, changeNickMsg, strlen(changeNickMsg), 0) <= 0) {
	pthread_mutex_unlock(&lock);
	sock_is_gone(socket);
	free(test);
	free(changeNickMsg);
	perror("Socket send() failed");
        close(socket);
        pthread_exit(NULL);
      }
      chanName* chans2send = get_channels_on_user(socket);
      while (chans2send != NULL) {
	send_to_rest_of_channel(chans2send->name, changeNickMsg, socket);
	chans2send = chans2send->next;
      }
    }
  }
  pthread_mutex_unlock(&lock);
  free(test);
  free(changeNickMsg);
  return 0;
}

int handle_USER (char* params, char* user, int socket) {
  int i;
  char* sub;
  char* nick;
  
  pthread_mutex_lock(&lock);
  if (!strlen(user) || (nick = get_nick_from_sock(socket)) == NULL || !strlen(nick)) {
    for (i = 0; params[i] != '\0' && params[i] != ' ' && i < MAX_USER_LEN; i++)
      user[i] = params[i];
    user[i] = '\0';
    pthread_mutex_unlock(&lock);
    sub = strstr(params, ":");
    if (sub !=NULL) {
      sub = strdup(sub+1);
      bzero(params, MAX_MSG_LEN+1);
      for (i = 0; sub[i] != '\0' && i < MAX_MSG_LEN; i++)
        params[i] = sub[i];
      params[i] = '\0';
    } else {
      params[0] = '\0';
    }
  } else {
    pthread_mutex_unlock(&lock);
    char* errorMsg = (char*)malloc(MAX_MSG_LEN+1);
    sprintf(errorMsg, ":%s 462 %s :You may not reregister\r\n",serverHost,nick);
    pthread_mutex_t clock = get_lock_from_sock(socket);
    pthread_mutex_lock(&clock);
    if (send(socket, errorMsg, strlen(errorMsg), 0) <= 0) {
      pthread_mutex_unlock(&clock);
      sock_is_gone(socket);
      remove_client(socket);
      free(errorMsg);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }  
    pthread_mutex_unlock(&clock);
    free(errorMsg);
  }
  return 0;
}
