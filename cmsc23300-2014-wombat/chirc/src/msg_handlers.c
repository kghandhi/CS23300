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
#include "msg_handlers.h"
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

int handle_PRIVMSG(char *args, char *placeholder, int socket) {
  int i = 0, j = 0;
  int top = 0,chan_oper = 0;
  int sent = 0;
  char* target= (char*)malloc(MAX_NICK_LEN+1);
  char* nick;
  char* user;
  char* host;  
  char* msg = (char*)malloc(MAX_MSG_LEN+1);
  char* fullmsg = (char*)malloc(MAX_MSG_LEN+1);
  int voice = 0,  mod = 0,  found_chan = 0;
  char* msgend = (char*)malloc(MAX_MSG_LEN+1);
  *msgend = '\0';
 
  pthread_mutex_lock (&lock);
  nick = get_nick_from_sock(socket);
  user = get_user_from_sock(socket);
  host = get_host_from_sock(socket);
  pthread_mutex_unlock(&lock);

  for (i = 0; args[i] != ' ' && args[i] != '\0' && i < MAX_NICK_LEN; i++){
    target[i] = args[i];
  }
  target[i] = '\0';
 
  char* format = ":!@ PRIVMSG \r\n";
  int len_restofMsg = strlen(nick) + strlen(user) +
    strlen(host) + strlen(target) + strlen(format);


  for (; i <= strlen(args) && i<MAX_MSG_LEN - len_restofMsg; i++){
    msg[j] = args[i];
    j++;
  }
  msg[j] = '\0';
  if (i <= strlen(args)) {
    int k;
    for (k = 0; i <= strlen(args) && i < MAX_MSG_LEN + 1; i++, k++)
      msgend[k] = args[i];
    msgend[k] = '\0';
  }
  
  pthread_mutex_lock(&lock);
  pthread_mutex_t slock = get_lock_from_sock(socket);
  pthread_mutex_unlock(&lock);

  if (target[0] == '#'){
    
    if (!is_already_chan(target)){
      char* err401 = (char*)malloc(MAX_MSG_LEN+1);
      sprintf(err401, ":%s 401 %s %s :No such nick/channel\r\n", serverHost,nick,target);
     
      pthread_mutex_lock(&slock);
      if (send(socket, err401, strlen(err401),0) <= 0) {
	pthread_mutex_unlock(&slock);
	sock_is_gone(socket);
	free(target);
	free(err401);
	free(msg);
	free(fullmsg);
	free(msgend);
	perror("Socket send() failed");
	close(socket);
	pthread_exit(NULL);
      }
      pthread_mutex_unlock(&slock);
      free(err401);
    
    } else {
      
      pthread_mutex_lock(&lock2);
      chanName* tmp = get_channels_on_user(socket);
      
      while (tmp != NULL){
	if (!strcmp(tmp->name, target)){	  
	  mod = get_modMode(target);
	  top = get_topMode(target);
	  voice = tmp->voice;
	  chan_oper = tmp->coper;
	 
	  
	  if ((voice && mod) | !mod | ((chan_oper | get_oper_from_sock(socket)) && mod) 
	      | (top && (chan_oper | get_oper_from_sock(socket)))){
	    if (!strlen(msgend)) {
	      sprintf(fullmsg, ":%s!%s@%s PRIVMSG %s%s\r\n", nick,user,host,target,msg);
	      pthread_mutex_unlock(&lock2);
	      send_to_rest_of_channel(target, fullmsg, socket);
	    } else {
	      sprintf(fullmsg, ":%s!%s@%s PRIVMSG %s%s\r\n", nick,user,host,target,msg);
	      sprintf(msgend, "%s\r\n", msgend);
	      pthread_mutex_unlock(&lock2);
	      send2_to_rest_of_channel(target, fullmsg, msgend, socket);
	    }
	    found_chan++;
	  }
	  break;
	}
	tmp = tmp->next;
      }
      pthread_mutex_unlock(&lock2);
      free(msgend);
      free(msg);
      free(fullmsg);
      
      if (!found_chan){
	char *err404 = (char*)malloc(sizeof(char)*(MAX_MSG_LEN+1));
	sprintf(err404, ":%s 404 %s %s :Cannot send to channel\r\n", serverHost, nick, target);

	pthread_mutex_lock(&slock);
	if (send(socket, err404, strlen(err404),0) <= 0) {
	  pthread_mutex_unlock(&slock);
	  sock_is_gone(socket);
	  free(target);
	  free(err404);
	  perror("Socket send() failed");
	  close(socket);
	  pthread_exit(NULL);
	}
	pthread_mutex_unlock(&slock);
	free(err404);
      } else {
     
	if (top && !(chan_oper | get_oper_from_sock(socket))){
	  char* err482 = (char*)malloc(sizeof(char)*(MAX_MSG_LEN+1));
	  sprintf(err482, ":%s 482 %s %s :You're not channel operator\r\n", serverHost, nick, target);
	  
	  pthread_mutex_lock(&slock);
	  if (send(socket,err482,strlen(err482),0) <= 0){
	    pthread_mutex_unlock(&slock);
	    sock_is_gone(socket);
	    free(target);
	    free(err482);
	    free(msg);
	    free(fullmsg);
	    free(msgend);
	    perror("Socket send() failed");
	    close(socket);
	    pthread_exit(NULL);
	  }
	  pthread_mutex_unlock(&slock);
	  free(err482);
	}
      }
    }
    free(target);
  } else {
    /* PRIVMSG to users*/
    
    pthread_mutex_lock (&lock);
    client* search = clients;
    while (search != NULL){
      if (!strcmp(search->nick,target)){
	pthread_mutex_unlock(&lock);
	pthread_mutex_t clock = get_lock_from_nick(target);
	if (!strlen(msgend)) {
	  sprintf(fullmsg, ":%s!%s@%s PRIVMSG %s%s\r\n", nick,user,host,target,msg);
	  pthread_mutex_lock(&clock);
	  if (send(search->sock, fullmsg, strlen(fullmsg),0) <= 0) {
	    pthread_mutex_unlock(&clock);
	    sock_is_gone(search->sock);
	    free(msg);
	    free(fullmsg);
	    free(msgend);
	    free(target);
	    perror("Socket send() failed");
	    close(search->sock);
	  }
	} else {
	  sprintf(fullmsg, ":%s!%s@%s PRIVMSG %s%s", nick,user,host,target,msg);
	  sprintf(msgend, "%s\r\n", msgend);
	  pthread_mutex_lock(&clock);
	  if (send(search->sock, fullmsg, strlen(fullmsg),0) <= 0) {
	    pthread_mutex_unlock(&clock);
	    sock_is_gone(search->sock);
	    free(msg);
	    free(fullmsg);
	    free(msgend);
	    free(target);
	    perror("Socket send() failed");
	    close(search->sock);
	  }
	  if (send(search->sock, msgend, strlen(msgend),0) <= 0) {
	    pthread_mutex_unlock(&clock);
	    sock_is_gone(search->sock);
	    free(msg);
	    free(fullmsg);
	    free(msgend);
	    free(target);
	    perror("Socket send() failed");
	    close(search->sock);
	  }
	}
	if(is_away(search->sock)) {
	  char* awaymsg = get_awayMsg(search->sock);
	  pthread_mutex_unlock(&clock);
	  char* awayRply = (char*)malloc(MAX_MSG_LEN+1);
	  sprintf(awayRply, ":%s 301 %s %s :%s\r\n", serverHost, nick, target, awaymsg);
	  pthread_mutex_lock(&slock);
	  if (send(socket, awayRply, strlen(awayRply),0) <= 0) {
	    pthread_mutex_unlock(&slock);
	    sock_is_gone(socket);
	    free(msg);
	    free(fullmsg);
	    free(msgend);
	    free(target);
	    free(awayRply);
	    perror("Socket send() failed");
	    close(search->sock);
	  }
	  pthread_mutex_unlock(&slock);
	  free(awayRply);
	} else {
	  pthread_mutex_unlock(&clock);
	}
	sent++;
	break;
      }
      search = search->next;
    }
    if (search == NULL)
      pthread_mutex_unlock (&lock);
    free(msg);
    free(fullmsg);
    free(msgend);
 
    if (!sent) {
      char* err401 = (char*)malloc(MAX_MSG_LEN+1);    
      sprintf(err401, ":%s 401 %s %s :No such nick/channel\r\n", serverHost,nick,target);
      pthread_mutex_lock(&slock); 
      if (send(socket, err401, strlen(err401),0) <= 0) {
	pthread_mutex_unlock(&slock);
	free(target);
	free(err401);
	perror("Socket send() failed");
	close(socket);
	pthread_exit(NULL);
      }
      pthread_mutex_unlock(&slock);
      free(err401);
    }
    free(target);
  }
  return 0;
}

//socket is the socket of the client who sent the message, other socket as below is a placeholder for the
// socket of the recipient. Same for the other function
int handle_NOTICE(char *args, char* placeholder, int socket) {
  int i = 0, j = 0, mod = 0, voice = 0;
  char* target = (char*)malloc(MAX_NICK_LEN+1);
  char* nick;
  char* text = (char*)malloc(MAX_MSG_LEN+1);
  char* fullmsg = (char*)malloc(MAX_MSG_LEN+1);
  char* user;
  char* host;
  char* msgend = (char*)malloc(MAX_MSG_LEN+1);
  *msgend = '\0';

  pthread_mutex_lock (&lock);
  nick = get_nick_from_sock(socket);
  user = get_user_from_sock(socket);
  host = get_host_from_sock(socket);
  pthread_mutex_unlock(&lock);

  for (; args[i] != ' ' && args[i] != '\0' && i < MAX_NICK_LEN; i++){
    target[i] = args[i];
  }
  target[i] = '\0';
 

  char* format = ":!@ NOTICE \r\n";

  int len_restofMsg = strlen(nick) + strlen(user) +
    strlen(host) + strlen(target) + strlen(format);

  for (;i <= strlen(args) && i < MAX_MSG_LEN - len_restofMsg; i++){
    text[j] = args[i];
    j++;
  }
  text[j] = '\0';
  if (i <= strlen(args)) {
    int k;
    for (k = 0; i <= strlen(args) && i < MAX_MSG_LEN + 1; i++, k++)
      msgend[k] = args[i];
    msgend[k] = '\0';
  }
  
  if (target[0] == '#'){
    
    pthread_mutex_lock(&lock2);
    chanName* tmp = get_channels_on_user(socket);
    while (tmp != NULL){
      if (tmp->name == target){

	mod = get_modMode(target);
	int chan_oper = tmp->coper;
	
	voice = tmp->voice;

	if ((voice && mod) | !mod | ((chan_oper | get_oper_from_sock(socket)) && mod)){
	  if (!strlen(msgend)) {
	    sprintf(fullmsg, ":%s!%s@%s NOTICE %s%s\r\n",nick,user,host,target,text);
	    pthread_mutex_unlock(&lock2);
	    send_to_rest_of_channel(target, fullmsg, socket);
	  } else {
	    sprintf(fullmsg, ":%s!%s@%s NOTICE %s%s",nick,user,host,target,text);
	    sprintf(msgend, "%s\r\n", msgend);
	     pthread_mutex_unlock(&lock2);
	    send2_to_rest_of_channel(target, fullmsg, msgend, socket);
	  }
	}
	break;
      }
      tmp = tmp->next;
    }
    pthread_mutex_unlock(&lock2);
  } else {
    pthread_mutex_lock(&lock);
    client* search = clients;
    while (search != NULL){
      if (!strcmp(search->nick,target)){
	pthread_mutex_unlock(&lock);
	pthread_mutex_t clock = get_lock_from_nick(target);
	if (!strlen(msgend)) {
	  
	  sprintf(fullmsg, ":%s!%s@%s NOTICE %s%s\r\n", nick,user,host,target,text);
	  pthread_mutex_lock(&clock);
	  if (send(search->sock,fullmsg,strlen(fullmsg),0) <= 0) {
	    pthread_mutex_unlock(&clock);
	    sock_is_gone(search->sock);
	    free(text);
	    free(target);
	    free(msgend);
	    free(fullmsg);
	    perror("Socket send() failed");
	    close(search->sock);
	  }
	} else {
	  sprintf(fullmsg, ":%s!%s@%s NOTICE %s%s", nick,user,host,target,text);
	  sprintf(fullmsg, ":%s\r\n", msgend);
	  pthread_mutex_lock(&clock);
	  if (send(search->sock,fullmsg,strlen(fullmsg),0) <= 0) {
	    pthread_mutex_unlock(&clock);
	    sock_is_gone(search->sock);
	    free(text);
	    free(target);
	    free(msgend);
	    free(fullmsg);
	    perror("Socket send() failed");
	    close(search->sock);
	  }
	  if (send(search->sock,msgend,strlen(msgend),0) <= 0) {
	    pthread_mutex_unlock(&clock);
	    sock_is_gone(search->sock);
	    free(text);
	    free(target);
	    free(msgend);
	    free(fullmsg);
	    perror("Socket send() failed");
	    close(search->sock);
	  }
	}
	pthread_mutex_unlock(&clock);
	break;
      }
      search = search->next;
    }
    pthread_mutex_unlock(&lock);
  }
  free(text);
  free(target);
  free(msgend);
  free(fullmsg);
  return 0;
}
