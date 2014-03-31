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
#include "channel_handlers.h"
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

/*############added handler for JOIN command############*/
int handle_JOIN(char* params, char* placeholder, int socket) {
  char* newChan = (char*)malloc(MAX_CHAN_LEN+1);
  int i;
  

  
  for (i = 0; params[i] != '\0' && params[i] != ' ' && i < MAX_CHAN_LEN; i++)
    newChan[i] = params[i];
  newChan[i] = '\0';

  if (is_chan_on_user(newChan, socket)) {
    free(newChan);
    return 0; 
  }

  pthread_mutex_lock(&lock);
  char* nickname = get_nick_from_sock(socket);
  char* username = get_user_from_sock(socket);
  char* hostname = get_host_from_sock(socket);
  pthread_mutex_t slock = get_lock_from_sock(socket);
  pthread_mutex_unlock(&lock);

  char* msg = (char*)malloc(MAX_MSG_LEN+1);

  sprintf(msg, ":%s!%s@%s JOIN %s\r\n", nickname, username, hostname, newChan);
  
  pthread_mutex_lock(&slock);
  if (send(socket, msg, strlen(msg), 0) <= 0) {
    pthread_mutex_unlock(&slock);
    sock_is_gone(socket);
    free(msg);
    free(newChan);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  pthread_mutex_unlock(&slock);

  if (has_topic(newChan)) {
    char* msg2 = (char*)malloc(MAX_MSG_LEN+1);
    sprintf(msg2, ":%s 332 %s %s :%s\r\n",
            serverHost, nickname, newChan, get_topic(newChan));
    pthread_mutex_lock(&slock);
    if (send(socket, msg2, strlen(msg2), 0) <= 0) {
      pthread_mutex_unlock(&slock);
      sock_is_gone(socket);
      free(msg);
      free(msg2);
      free(newChan);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    pthread_mutex_unlock(&slock);
    free(msg2);
  }

  pthread_mutex_lock(&lock2);
  if (is_already_chan(newChan))
    send_to_channel(newChan, msg);
  else
    add_channel(newChan);
  
  add_chanName(socket, newChan);
  add_chanUser(newChan, nickname, socket);
  pthread_mutex_unlock(&lock2);

  handle_NAMES(newChan, placeholder, socket);

  free(msg);
  free(newChan);

  return 0;
}

/*############added handler for PART command############*/
int handle_PART(char* params, char* placeholder, int socket) {
  char* chan2Part = (char*)malloc(MAX_CHAN_LEN+1);
  int i;
  
  for (i = 0; params[i] != '\0' && params[i] != ' ' && i < MAX_CHAN_LEN; i++)
    chan2Part[i] = params[i];
  chan2Part[i] = '\0';

  pthread_mutex_lock(&lock);
  char* nickname = get_nick_from_sock(socket);
  char* username = get_user_from_sock(socket);
  char* hostname = get_host_from_sock(socket);
  pthread_mutex_t slock = get_lock_from_sock(socket);
  pthread_mutex_unlock(&lock);

  pthread_mutex_lock(&lock2);
  int chansts = is_chan_on_user(chan2Part, socket);
  pthread_mutex_unlock(&lock2);

  if(!chansts) {
    char* errMsg = (char*)malloc(MAX_MSG_LEN+1);
    if (is_already_chan(chan2Part))
      sprintf(errMsg, ":%s 442 %s %s :You're not on that channel\r\n",
	      serverHost, nickname, chan2Part);
    else
      sprintf(errMsg, ":%s 403 %s %s :No such channel\r\n",
	      serverHost, nickname, chan2Part);
    
    pthread_mutex_lock(&slock);
    if (send(socket, errMsg, strlen(errMsg), 0) <= 0) {
      pthread_mutex_unlock(&slock);
      sock_is_gone(socket);
      free(errMsg);
      free(chan2Part);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    pthread_mutex_unlock(&slock);
    free(errMsg);
    free(chan2Part);
    return 0;
  }
  
  char* reason;
  char* isReason = strstr(params+i, ":");
  if (isReason != NULL)
    reason = strdup(isReason+1);
  else
    reason = "";
  
  char* msg = (char*)malloc(MAX_MSG_LEN+1);
  if (strlen(reason))
    sprintf(msg, ":%s!%s@%s PART %s :%s\r\n",
	    nickname, username, hostname, chan2Part, reason);
  else
    sprintf(msg, ":%s!%s@%s PART %s\r\n",
            nickname, username, hostname, chan2Part);

  send_to_channel(chan2Part, msg);
  
  pthread_mutex_lock(&lock2);
  remove_chanUser(chan2Part, socket);
  remove_chanName(socket, chan2Part);

  if (is_empty_channel(chan2Part))
    remove_channel(chan2Part);
  pthread_mutex_unlock(&lock2);


  if (isReason != NULL)
    free(reason);
  free(msg);
  free(chan2Part);
  return 0;
}

/*############added handler for TOPIC command############*/
int handle_TOPIC(char* params, char* placeholder, int socket) {
  char* chan2Topic = (char*)malloc(MAX_CHAN_LEN+1);
  int i;
  int top = 0;
  int voice = 0;
  int coper = 0;
  int oper = 0;
 

  for (i = 0; params[i] != '\0' && params[i] != ' ' && i < MAX_CHAN_LEN; i++)
    chan2Topic[i] = params[i];
  chan2Topic[i] = '\0';

  pthread_mutex_lock(&lock);
  char* nickname = get_nick_from_sock(socket);
  char* username = get_user_from_sock(socket);
  char* hostname = get_host_from_sock(socket);
  pthread_mutex_t slock = get_lock_from_sock(socket);
  pthread_mutex_unlock(&lock);


  pthread_mutex_lock(&lock2);
  int chansts = is_chan_on_user(chan2Topic, socket);
  pthread_mutex_unlock(&lock2);


  if(!chansts) {
    char* errMsg = (char*)malloc(MAX_MSG_LEN+1);
    sprintf(errMsg, ":%s 442 %s %s :You're not on that channel\r\n",
	    hostname, nickname, chan2Topic);

    pthread_mutex_lock(&slock);
    if (send(socket, errMsg, strlen(errMsg), 0) <= 0) {
      pthread_mutex_unlock(&slock);
      sock_is_gone(socket);
      free(errMsg);
      free(chan2Topic);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    pthread_mutex_unlock(&slock);
    free(errMsg);
    free(chan2Topic);
    return 0;
  }

  pthread_mutex_lock(&lock2);
  top = get_topMode(chan2Topic);
  voice = get_voice_from_sock(socket, chan2Topic);
  coper =  get_coper_from_sock(socket, chan2Topic);
  oper = get_oper_from_sock(socket);
  pthread_mutex_t chlock = get_lock_from_chan(chan2Topic);
  pthread_mutex_unlock(&lock2);

  int can_change = voice | !top | coper | oper;
  
  char* isChange = strstr(params+i, ":");
  if (isChange != NULL && can_change) {
    
    if (*(isChange + 1) != '\0') {
	char* newTopic = isChange + 1;
	
	pthread_mutex_lock(&chlock);
	change_topic(chan2Topic, newTopic);
	pthread_mutex_unlock(&chlock);

	char* msg = (char*)malloc(MAX_MSG_LEN+1);
	
	sprintf(msg, ":%s!%s@%s TOPIC %s :%s\r\n",
		nickname, username, hostname, chan2Topic, newTopic);
	send_to_channel(chan2Topic, msg);
	free(msg);
	
    } else {
      pthread_mutex_lock(&chlock);
      remove_topic(chan2Topic);
      pthread_mutex_unlock(&chlock);
      char* msg = (char*)malloc(MAX_MSG_LEN+1);
      sprintf(msg, ":%s!%s@%s TOPIC %s :\r\n",
	      nickname, username, hostname, chan2Topic);
      send_to_channel(chan2Topic, msg);
      free(msg);
	
    }
  } else if (isChange != NULL) { 

    char* err482 = (char*)malloc(sizeof(char)*(MAX_MSG_LEN+1));
    sprintf(err482, ":%s 482 %s %s :You're not channel operator\r\n", serverHost, nickname, chan2Topic);
    
    pthread_mutex_lock(&slock);
    if (send(socket, err482, strlen(err482), 0) <= 0) {
      pthread_mutex_unlock(&slock);
      sock_is_gone(socket);
      free(err482);
      free(chan2Topic);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    pthread_mutex_unlock(&slock);
    free(err482);
  } else {
   
    char* msg = (char*)malloc(MAX_MSG_LEN+1);
    
    pthread_mutex_lock(&chlock);
    char* topic = get_topic(chan2Topic);
    pthread_mutex_unlock(&chlock);

    if (topic != NULL)
      sprintf(msg, ":%s 332 %s %s :%s\r\n",
	      serverHost, nickname, chan2Topic, topic);
    else
      sprintf(msg, ":%s 331 %s %s :No topic is set\r\n",
	      serverHost, nickname, chan2Topic);
    pthread_mutex_lock(&slock);
    if (send(socket, msg, strlen(msg), 0) <= 0) {
      pthread_mutex_unlock(&slock);
      sock_is_gone(socket);
      free(msg);
      free(chan2Topic);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    pthread_mutex_unlock(&slock);
    free(msg);   
  }
  
  free(chan2Topic);
  return 0;
}
