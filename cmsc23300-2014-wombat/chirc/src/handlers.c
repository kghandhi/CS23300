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
#include "handlers.h"
#include "modes.h"
#include "away.h"
#include "topic.h"
#include "get.h"
#include "globals.h"

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
  
  pthread_mutex_t slock = get_lock_from_sock(socket);
  
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
	      send_to_rest_of_channel(target, fullmsg, socket);
	    } else {
	      sprintf(fullmsg, ":%s!%s@%s PRIVMSG %s%s\r\n", nick,user,host,target,msg);
	      sprintf(msgend, "%s\r\n", msgend);
	      send2_to_rest_of_channel(target, fullmsg, msgend, socket);
	    }
	    found_chan++;
	  }
	  break;
	}
	tmp = tmp->next;
      }
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
	  
	  if (send(socket,err482,strlen(err482),0) <= 0){
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
    
    chanName* tmp = get_channels_on_user(socket);
    while (tmp != NULL){
      if (tmp->name == target){

	mod = get_modMode(target);
	int chan_oper = tmp->coper;
	
	voice = tmp->voice;

	if ((voice && mod) | !mod | ((chan_oper | get_oper_from_sock(socket)) && mod)){
	  if (!strlen(msgend)) {
	    sprintf(fullmsg, ":%s!%s@%s NOTICE %s%s\r\n",nick,user,host,target,text);
	    send_to_rest_of_channel(target, fullmsg, socket);
	  } else {
	    sprintf(fullmsg, ":%s!%s@%s NOTICE %s%s",nick,user,host,target,text);
	    sprintf(msgend, "%s\r\n", msgend);
	    send2_to_rest_of_channel(target, fullmsg, msgend, socket);
	  }
	}
	break;
      }
      tmp = tmp->next;
    }
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

//note, before sending to the handler make sure if function call == PONG, drop silently.
//Further socket is the client who sent the PING message to the server
int handle_PING(char *placeholder, char *holdplacer, int socket){
  char* msg = (char*)malloc(MAX_MSG_LEN+1);

  sprintf(msg,"PONG %s\r\n",serverHost);
  pthread_mutex_t slock = get_lock_from_sock(socket);
  pthread_mutex_lock(&slock);
  if (send(socket,msg,strlen(msg),0) <= 0) {
    pthread_mutex_unlock(&slock);
    sock_is_gone(socket);
    free(msg);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  pthread_mutex_unlock(&slock);
  free(msg);
  return 0;
}

//Drops pong messages silently
int handle_PONG(char *placeholder, char *holdplacer, int socket){
  return 0;
}

//in this case socket is the client who just logged on
int handle_MOTD(char* placeholder, char* holdplacer, int socket){
  FILE* f = fopen("motd.txt","r");
  char* errmsg = (char*)malloc(MAX_MSG_LEN+1);
  char* msg = (char*)malloc(MAX_MSG_LEN+1);
  char* msg1 = (char*)malloc(MAX_MSG_LEN+1);
  char* msg2 = (char*)malloc(MAX_MSG_LEN+1);
  char* msg3 = (char*)malloc(MAX_MSG_LEN+1);

  pthread_mutex_lock(&lock);
  char* nick = get_nick_from_sock(socket);
  pthread_mutex_unlock(&lock);
  *msg1 = '\0';
  pthread_mutex_t clock =  get_lock_from_sock(socket);
  pthread_mutex_lock(&clock);
  if (!f){
    sprintf(errmsg, ":%s 422 %s :MOTD File is missing\r\n", serverHost,nick);
    if (send(socket, errmsg, strlen(errmsg),0) <= 0) {
      pthread_mutex_unlock(&clock);
      sock_is_gone(socket);
      free(errmsg);
      free(msg);
      free(msg1);
      free(msg2);
      free(msg3);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    free(errmsg);
    free(msg);
    free(msg1);
    free(msg2);
  } else {
    free(errmsg);
    sprintf(msg1, ":%s 375 %s :- %s Message of the day - \r\n",serverHost,nick,serverHost);
    if (send(socket,msg1,strlen(msg1),0) <= 0) {
      pthread_mutex_unlock(&clock);
      sock_is_gone(socket);
      free(msg);
      free(msg1);
      free(msg2);
      free(msg3);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    free(msg1);
    int len_restofMsg = strlen(serverHost) + strlen(nick) +
      strlen(": 372  :- \r\n");
    int space4msg = MAX_MSG_LEN - len_restofMsg;
    while (fgets(msg,space4msg,f) != NULL){
      sprintf(msg2, ":%s 372 %s :- %s\r\n", serverHost,nick,msg);
      if (send(socket,msg2,strlen(msg2),0) <= 0) {
	pthread_mutex_unlock(&clock);
	sock_is_gone(socket);
	free(msg);
	free(msg2);
	free(msg3);
        perror("Socket send() failed");
        close(socket);
        pthread_exit(NULL);
      }
    }
    free(msg);
    free(msg2);
    sprintf(msg3, ":%s 376 %s :End of MOTD command\r\n",serverHost,nick);
    if (send(socket,msg3,strlen(msg3),0) <= 0) {
      pthread_mutex_unlock(&clock);
      sock_is_gone(socket);
      free(msg3);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    free(msg3);
    fclose(f);
  }
  pthread_mutex_unlock(&clock);
  return 0;
}
 
//this should also be the socket of the client who just logged on
int handle_LUSERS(char* placeholder, char* holdplacer, int socket) {
  int num_unk_users = 0, num_reg_users = 0;
  
  pthread_mutex_lock(&lock);
  char* nick = get_nick_from_sock(socket); 
  num_reg_users = registeredClients;
  num_unk_users = unregisteredClients;

  pthread_mutex_unlock(&lock);
  
  char* msg1 = (char*)malloc(MAX_MSG_LEN+1);
  sprintf(msg1, ":%s 251 %s :There are %d users and %d services on %d servers\r\n", serverHost,nick,num_reg_users, 0, 1);
  
  char* msg2 = (char*)malloc(MAX_MSG_LEN+1);
  sprintf(msg2, ":%s 252 %s %d :operator(s) online\r\n", serverHost,nick,0);
  
  char *msg3 = (char*)malloc(MAX_MSG_LEN+1);
  sprintf(msg3, ":%s 253 %s %d :unknown connection(s)\r\n", serverHost,nick,num_unk_users);
  
  char *msg4 = (char*)malloc(MAX_MSG_LEN+1);
  sprintf(msg4, ":%s 254 %s %d :channels formed\r\n", serverHost,nick,0);
  
  char *msg5 = (char*)malloc(MAX_MSG_LEN+1);
  sprintf(msg5, ":%s 255 %s :I have %d clients and %d servers\r\n", serverHost,nick,num_unk_users + num_reg_users,1);

  pthread_mutex_t clock = get_lock_from_sock(socket);
  pthread_mutex_lock(&clock);
  if (send(socket,msg1,strlen(msg1),0) <= 0) {
    pthread_mutex_unlock(&clock);
    sock_is_gone(socket);
    free(msg1);
    free(msg2);
    free(msg3);
    free(msg4);
    free(msg5);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  free(msg1);
  if (send(socket,msg2,strlen(msg2),0) <= 0) {
    pthread_mutex_unlock(&clock);
    sock_is_gone(socket);
    free(msg2);
    free(msg3);
    free(msg4);
    free(msg5);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  free(msg2);
  if (send(socket,msg3,strlen(msg3),0) <= 0) {
    pthread_mutex_unlock(&clock);
    sock_is_gone(socket);
    free(msg3);
    free(msg4);
    free(msg5);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  free(msg3);
  if (send(socket,msg4,strlen(msg4),0) <= 0) {
    pthread_mutex_unlock(&clock);
    sock_is_gone(socket);
    free(msg4);
    free(msg5);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  free(msg4);
  if (send(socket,msg5,strlen(msg5),0) <= 0) {
    pthread_mutex_unlock(&clock);
    sock_is_gone(socket);
    free(msg5);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  pthread_mutex_unlock(&clock);
  free(msg5);
  return 0;
}

int handle_WHOIS (char* args, char* placeholder, int socket){
  int i = 0;
  int found = 0;
  char *mask = (char*)malloc(MAX_NICK_LEN+1);
  char* errmsg = (char*)malloc(MAX_MSG_LEN+1);
  client* search = clients;
  
  pthread_mutex_lock(&lock);
  char* nick = get_nick_from_sock(socket);
  if (!strlen(args)) {
    mask = nick;
  } else {
    for (; args[i] != ' ' && args[i] != '\0' && i < MAX_NICK_LEN; i++)
      mask[i] = args[i];
    mask[i] = '\0';
  }
  if (search == NULL) {
    pthread_mutex_unlock(&lock);
    free(mask);
    free(errmsg);
    return 0;
  }
  while (search != NULL){
    if (!strcmp(mask,search->nick)) {
      found++;
      break;
    }
    search = search->next;
  }
  pthread_mutex_unlock(&lock);
  pthread_mutex_t clock = get_lock_from_sock(socket);
  if (!found){
    pthread_mutex_lock(&clock);
    sprintf(errmsg, ":%s 401 %s %s :No such nick/channel\r\n", serverHost,nick,mask);
    if (send(socket, errmsg, strlen(errmsg),0) <= 0) {
      pthread_mutex_unlock(&clock);
      sock_is_gone(socket);
      free(mask);
      free(errmsg);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    pthread_mutex_unlock(&clock);
    free(mask);
    free(errmsg);
    return 0;
  }
  free(errmsg);
 
 

  char* msg1 = (char*)malloc(MAX_MSG_LEN+1);
  sprintf(msg1, ":%s 311 %s %s %s %s * :%s\r\n", serverHost, nick,mask,search->user,search->host,search->real);
  
  char* msg2 = (char*)malloc(MAX_MSG_LEN+1);
  sprintf(msg2,":%s 312 %s %s %d :%s\r\n",serverHost,nick,mask,1,"chirc-0.3.2");
  
  char* msg3 = (char*)malloc(MAX_MSG_LEN+1);
  sprintf(msg3, ":%s 318 %s %s :End of WHOIS list\r\n", serverHost,nick,mask);
  
  pthread_mutex_unlock(&lock);

  pthread_mutex_lock(&clock);
  if (send(socket,msg1,strlen(msg1),0) <= 0) {
    pthread_mutex_unlock(&clock);
    sock_is_gone(socket);
    free(mask);
    free(msg1);
    free(msg2);
    free(msg3);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  free(msg1);

  ////////////////////NOTINSIDELOCK/////////////////////////////////
   
  pthread_mutex_unlock(&clock);

  chanName* nickChans = get_channels_on_user(search->sock);
  if (nickChans != NULL) {
    char* msg4 = (char*)malloc(MAX_MSG_LEN+1);
    sprintf(msg4, ":%s 319 %s %s :", serverHost, nick, mask);
    while(nickChans != NULL) {
      if (get_coper_from_sock(search->sock, nickChans->name)) {
        strcat(msg4, "@");
      } else if (get_voice_from_sock(search->sock, nickChans->name)) {
        strcat(msg4, "+");
      }
      strcat(msg4, nickChans->name);
      strcat(msg4, " ");
      nickChans = nickChans->next;
    }
    strcat(msg4, "\r\n");

    if (send(socket,msg4,strlen(msg4),0) <= 0) {
      sock_is_gone(socket);
      free(mask);
      free(msg2);
      free(msg3);
      free(msg4);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    free(msg4);
  }

  pthread_mutex_lock(&clock);

  //////////////////////////////////////////////////////////////////

  if (send(socket,msg2,strlen(msg2),0) <= 0) {
    pthread_mutex_unlock(&clock);
    sock_is_gone(socket);
    free(mask);
    free(msg2);
    free(msg3);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  free(msg2);

  ////////////////////NOTINSIDELOCK/////////////////////////////////
   
  pthread_mutex_unlock(&clock);

  if (is_away(search->sock)) {
    char* msg5 = (char*)malloc(MAX_MSG_LEN+1);
    sprintf(msg5, ":%s 301 %s %s :%s\r\n", serverHost, nick, mask, get_awayMsg(search->sock));
    if (send(socket,msg5,strlen(msg5),0) <= 0) {
      sock_is_gone(socket);
      free(mask);
      free(msg3);
      free(msg5);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    free(msg5);
  }

  if (get_oper_from_sock(search->sock)) {
    char* msg6 = (char*)malloc(MAX_MSG_LEN+1);
    sprintf(msg6, ":%s 313 %s %s :is an IRC operator\r\n", serverHost, nick, mask);
    if (send(socket,msg6,strlen(msg6),0) <= 0) {
      sock_is_gone(socket);
      free(mask);
      free(msg3);
      free(msg6);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    free(msg6);
  }

  pthread_mutex_lock(&clock);

  //////////////////////////////////////////////////////////////////
  if (send(socket,msg3,strlen(msg3),0) <= 0) {
    pthread_mutex_unlock(&clock);
    sock_is_gone(socket);
    free(mask);
    free(msg3);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  pthread_mutex_lock(&clock);
  free(mask);
  free(msg3);
  return 0;
}

int handle_QUIT(char* params, char* placeholder, int socket) {
  char* reason;
  char* quitMsg = (char*)malloc(MAX_MSG_LEN+1);
  
  pthread_mutex_lock(&lock);
  char* hostname = get_host_from_sock(socket);
  pthread_mutex_unlock(&lock);

  if(*params == ':')
    reason = strdup(params + 1);
  else
    reason = strdup("Client Quit");
  *quitMsg = '\0';
  sprintf(quitMsg, "ERROR :Closing Link: %s (%s)\r\n", hostname, reason);
  pthread_mutex_t clock = get_lock_from_sock(socket);
  pthread_mutex_lock(&clock);
  if (send(socket, quitMsg, strlen(quitMsg), 0) <= 0) {
    pthread_mutex_unlock(&clock);
    sock_is_gone(socket);
    free(quitMsg);
    free(reason);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  pthread_mutex_unlock(&clock);
  free(quitMsg);
  
  chanName* chans2send = get_channels_on_user(socket);
  char* globalQuitMsg = (char*)malloc(MAX_MSG_LEN+1);
  char* nickname = get_nick_from_sock(socket);
  char* username = get_user_from_sock(socket);
  sprintf(globalQuitMsg, ":%s!%s@%s QUIT :%s\r\n",
 nickname, username, hostname, reason);
  while (chans2send != NULL) {
    remove_chanUser(chans2send->name, socket);
    send_to_channel(chans2send->name, globalQuitMsg);
    chans2send = chans2send->next;
  }

  pthread_mutex_lock(&lock);
  registeredClients -= 1;
  pthread_mutex_unlock(&lock);
  
  remove_client(socket);
  free(globalQuitMsg);
  free(reason);
  close(socket);
  pthread_exit(NULL);
}

/*############added handler for NAMES command############*/
/*WARNING: no locking implemented*/
/*NOTE: This is not placed in the code in order of its command number*/
/*with respect to the other commands because it is called by handle_JOIN*/
int handle_NAMES(char* params, char* placeholder, int socket) {
  char* nickname = get_nick_from_sock(socket);
  int i;
  for (i = 0; params[i] == ' '; i++)
    ;
  if (params[i] == '\0') {
    channel* chans = channels;
    while (chans != NULL) {
      char* chan2Names = chans->name;
      chanUser* search = get_users_on_channel(chan2Names);
      //NOTE: As said below, try to make work for userlists longer than 512 if we have time
      char* userlist = (char*)malloc(MAX_MSG_LEN+1);
      userlist[0] = '\0';
      while (search != NULL) {
	if (search->coper)
	  strcat(userlist, "@");
	else if (search->voice)
	  strcat(userlist, "+");
	strcat(userlist, search->nick);
	if (search->next != NULL)
	  strcat(userlist, " ");
	search = search->next;
      }
      char* msg = (char*)malloc(MAX_MSG_LEN+1);
      sprintf(msg, ":%s 353 %s = %s :%s\r\n",
	      serverHost, nickname, chan2Names, userlist);
      if (send(socket, msg, strlen(msg), 0) <= 0) {
	sock_is_gone(socket);
	free(msg);
	free(userlist);
	perror("Socket send() failed");
	close(socket);
	pthread_exit(NULL);
      }
      free(msg);
      free(userlist);
      chans = chans->next;
    }
    client* peeps = clients;
    //NOTE: also make this work with arbitrary numbers of users
    char* channelless = (char*)malloc(MAX_MSG_LEN+1);
    channelless[0] = '\0';
    while(peeps != NULL) {
      if (peeps->chans == NULL) {
	if (strlen(channelless))
	  strcat(channelless, " ");
	strcat(channelless, peeps->nick);
      }
      peeps = peeps->next;
    }
    if(strlen(channelless)) {
      char* msg2 = (char*)malloc(MAX_MSG_LEN+1);
      sprintf(msg2, ":%s 353 %s * * :%s\r\n",
	      serverHost, nickname, channelless);
      if (send(socket, msg2, strlen(msg2), 0) <= 0) {
	sock_is_gone(socket);
	free(msg2);
	free(channelless);
	perror("Socket send() failed");
	close(socket);
	pthread_exit(NULL);
      }
      free(msg2);
    }
    free(channelless);
    char* msg3 = (char*)malloc(MAX_MSG_LEN+1);
    sprintf(msg3, ":%s 366 %s * :End of NAMES list\r\n",
	    serverHost, nickname);
    if (send(socket, msg3, strlen(msg3), 0) <= 0) {
      sock_is_gone(socket);
      free(msg3);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    free(msg3);
  } else {
    char* chan2Names = (char*)malloc(MAX_CHAN_LEN+1);
    for(; params[i] != ' ' && params[i] != '\0'; i++)
      chan2Names[i] = params[i];
    chan2Names[i] = '\0';
    
    chanUser* search = get_users_on_channel(chan2Names);
    //NOTE: If we have time, we should make this work if the userlist is really super long and doesn't fit in one message
    char* userlist = (char*)malloc(MAX_MSG_LEN+1);
    userlist[0] = '\0';
    while (search != NULL) {
      if (search->coper)
	strcat(userlist, "@");
      else if (search->voice)
	strcat(userlist, "+");
      strcat(userlist, search->nick);
      if (search->next != NULL)
	strcat(userlist, " ");
      search = search->next;
    }
    if (strlen(userlist)) {
      char* msg = (char*)malloc(MAX_MSG_LEN+1);
      sprintf(msg, ":%s 353 %s = %s :%s\r\n",
	      serverHost, nickname, chan2Names, userlist);
      if (send(socket, msg, strlen(msg), 0) <= 0) {
	sock_is_gone(socket);
	free(msg);
	free(chan2Names);
	free(userlist);
	perror("Socket send() failed");
	close(socket);
	pthread_exit(NULL);
      }
      free(msg);
    }
    free(userlist);
    char* msg2 = (char*)malloc(MAX_MSG_LEN+1);
    sprintf(msg2, ":%s 366 %s %s :End of NAMES list\r\n", serverHost, nickname,chan2Names);
    if (send(socket, msg2, strlen(msg2), 0) <= 0) {
      sock_is_gone(socket);
      free(msg2);
      free(chan2Names);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    free(msg2);
    free(chan2Names);
  }
  return 0;
}


/*############added handler for JOIN command############*/
/*WARNING: no locking implemented*/
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

  char* nickname = get_nick_from_sock(socket);
  char* username = get_user_from_sock(socket);
  char* hostname = get_host_from_sock(socket);
  char* msg = (char*)malloc(MAX_MSG_LEN+1);

  sprintf(msg, ":%s!%s@%s JOIN %s\r\n", nickname, username, hostname, newChan);
  if (send(socket, msg, strlen(msg), 0) <= 0) {
    sock_is_gone(socket);
    free(msg);
    free(newChan);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }

  if (has_topic(newChan)) {
    char* msg2 = (char*)malloc(MAX_MSG_LEN+1);
    sprintf(msg2, ":%s 332 %s %s :%s\r\n",
            serverHost, nickname, newChan, get_topic(newChan));
    if (send(socket, msg2, strlen(msg2), 0) <= 0) {
      sock_is_gone(socket);
      free(msg);
      free(msg2);
      free(newChan);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    free(msg2);
  }

  if (is_already_chan(newChan))
    send_to_channel(newChan, msg);
  else
    add_channel(newChan);
  
  add_chanName(socket, newChan);
  add_chanUser(newChan, nickname, socket);
  
  handle_NAMES(newChan, placeholder, socket);

  free(msg);
  free(newChan);

  return 0;
}

/*############added handler for PART command############*/
/*WARNING: no locking implemented*/
int handle_PART(char* params, char* placeholder, int socket) {
  char* chan2Part = (char*)malloc(MAX_CHAN_LEN+1);
  int i;
  for (i = 0; params[i] != '\0' && params[i] != ' ' && i < MAX_CHAN_LEN; i++)
    chan2Part[i] = params[i];
  chan2Part[i] = '\0';

  char* nickname = get_nick_from_sock(socket);
  char* username = get_user_from_sock(socket);
  char* hostname = get_host_from_sock(socket);

  if(!is_chan_on_user(chan2Part, socket)) {
    char* errMsg = (char*)malloc(MAX_MSG_LEN+1);
    if (is_already_chan(chan2Part))
      sprintf(errMsg, ":%s 442 %s %s :You're not on that channel\r\n",
	      serverHost, nickname, chan2Part);
    else
      sprintf(errMsg, ":%s 403 %s %s :No such channel\r\n",
	      serverHost, nickname, chan2Part);
    if (send(socket, errMsg, strlen(errMsg), 0) <= 0) {
      sock_is_gone(socket);
      free(errMsg);
      free(chan2Part);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
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
  remove_chanUser(chan2Part, socket);
  remove_chanName(socket, chan2Part);

  if (is_empty_channel(chan2Part))
    remove_channel(chan2Part);
  
  if (isReason != NULL)
    free(reason);
  free(msg);
  free(chan2Part);
  return 0;
}

/*############added handler for TOPIC command############*/
/*WARNING: no locking implemented*/
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

  char* nickname = get_nick_from_sock(socket);
  char* username = get_user_from_sock(socket);
  char* hostname = get_host_from_sock(socket);

  if(!is_chan_on_user(chan2Topic, socket)) {
    char* errMsg = (char*)malloc(MAX_MSG_LEN+1);
    sprintf(errMsg, ":%s 442 %s %s :You're not on that channel\r\n",
	    hostname, nickname, chan2Topic);
    if (send(socket, errMsg, strlen(errMsg), 0) <= 0) {
      sock_is_gone(socket);
      free(errMsg);
      free(chan2Topic);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    free(errMsg);
    free(chan2Topic);
    return 0;
  }

  
  top = get_topMode(chan2Topic);
  voice = get_voice_from_sock(socket, chan2Topic);
  coper =  get_coper_from_sock(socket, chan2Topic);
  oper = get_oper_from_sock(socket);
  int can_change = voice | !top | coper | oper;
  

  char* isChange = strstr(params+i, ":");
  if (isChange != NULL && can_change) {
    if (*(isChange + 1) != '\0') {
	char* newTopic = isChange + 1;
	change_topic(chan2Topic, newTopic);
	char* msg = (char*)malloc(MAX_MSG_LEN+1);
	
	sprintf(msg, ":%s!%s@%s TOPIC %s :%s\r\n",
		nickname, username, hostname, chan2Topic, newTopic);
	send_to_channel(chan2Topic, msg);
	free(msg);
	
    } else {
      remove_topic(chan2Topic);
      char* msg = (char*)malloc(MAX_MSG_LEN+1);
      sprintf(msg, ":%s!%s@%s TOPIC %s :\r\n",
	      nickname, username, hostname, chan2Topic);
      send_to_channel(chan2Topic, msg);
      free(msg);
	
    }
  } else if (isChange != NULL) { 
    char* err482 = (char*)malloc(sizeof(char)*(MAX_MSG_LEN+1));
    sprintf(err482, ":%s 482 %s %s :You're not channel operator\r\n", serverHost, nickname, chan2Topic);
    if (send(socket, err482, strlen(err482), 0) <= 0) {
      sock_is_gone(socket);
      free(err482);
      free(chan2Topic);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    free(err482);
  } else {
    char* msg = (char*)malloc(MAX_MSG_LEN+1);
    char* topic = get_topic(chan2Topic);
    if (topic != NULL)
      sprintf(msg, ":%s 332 %s %s :%s\r\n",
	      serverHost, nickname, chan2Topic, topic);
    else
      sprintf(msg, ":%s 331 %s %s :No topic is set\r\n",
	      serverHost, nickname, chan2Topic);
    if (send(socket, msg, strlen(msg), 0) <= 0) {
      sock_is_gone(socket);
      free(msg);
      free(chan2Topic);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    free(msg);   
  }
  
  free(chan2Topic);
  return 0;
}

/* args contains a <name> <password> ignore name */
/* error: ERR_PASSWDMISMATCH */
/* RPL_YOUREOPER */
int handle_OPER(char* args, char* placeholder, int socket){
  int i = 0, j = 0;
  char *user = (char*)malloc(sizeof(char)*MAX_USER_LEN);
  char *password = (char*)malloc(sizeof(char)*MAX_PWD_LEN);
  char *nick;

  for (; args[i] != ' ' && args[i] != '\0'; i++){
    user[i] = args[i];
  }
  user[i] = '\0';
  i++;
  for (; i <= strlen(args); i++, j++){
    password[j] = args[i];
  }
  password[j] = '\0';
  
  pthread_mutex_lock(&lock);
  nick = get_nick_from_sock(socket);
  pthread_mutex_unlock(&lock);
  
  if (!password | !user){ //drop it silently? The reference says its an error but were not required to do it
    return 0;
  }

  pthread_mutex_t clock = get_lock_from_sock(socket);

  if (strcmp(passwd, password)) {
    char *err464 = (char*)malloc(sizeof(char)*(MAX_NICK_LEN+MAX_HOST_LEN));
    sprintf(err464, ":%s 464 %s :Password incorrect\r\n", serverHost, nick);
   
    pthread_mutex_lock(&clock);
    if (send(socket,err464,strlen(err464),0) <= 0) {
      pthread_mutex_unlock(&clock);
      sock_is_gone(socket);
      free(err464);
      free(user);
      free(password);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    } 
    pthread_mutex_unlock(&clock);
    free(err464);
  } else {

    pthread_mutex_lock(&lock);
    make_opp(socket);
    pthread_mutex_unlock(&lock);

    char *reply = (char*)malloc(sizeof(char)*(MAX_NICK_LEN+MAX_HOST_LEN));
    sprintf(reply, ":%s 381 %s :You are now an IRC operator\r\n", serverHost, nick);
    pthread_mutex_lock(&clock);
    if (send(socket,reply,strlen(reply),0) <= 0) {
      pthread_mutex_unlock(&clock);
      sock_is_gone(socket);
      free(reply);
      free(user);
      free(password);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    } 
    pthread_mutex_unlock(&clock);
    free(reply);
  }
  free(user);
  free(password);
  return 0;
}

/* args shoud be a NICK and a two character mode string "-o" or "+o" */
/* ERR_UMODEUNKNOWNFLAG and ERR_USERSDONTMATCH replies */
/* If success, relay the message prefixed by user's nick and the mode string */ 
int handle_MODE(char* args, char* placeholder, int socket){
  int i = 0, j = 0, k = 0;
  char *chan = NULL;
  char *nick;
  char *mode = NULL;
  char *user;
  char *host;
  
  /* USER MODE */
  char *err501 = NULL; 
  char *err502 = NULL;
  char *umreply =  NULL;
  
  /* CHANNEL MODE */
  char *cmreply = NULL; 
  char *err403 = NULL; 
  char *err482 = NULL; 
  char *err472 = NULL; 
  char *modes = NULL;

  /* member status */
  char *msreply = NULL; 
  char *member = NULL; 
  char *err441 = NULL;


  /* all the modes on a given channel formatted to be sent out*/
  //channel* search = channels;  
 
  if (args[0] == '#'){
    chan = (char*)malloc(sizeof(char)*MAX_CHAN_LEN);
    for (; args[i] != ' ' && args[i] != '\0'; i++){
      chan[i] = args[i];
    }
    chan[i] = '\0';
    i++;
    if (i <= strlen(args)){
      mode = (char*)malloc(sizeof(char)*MODE_LEN*3);
      for (; args[i] != ' ' && args[i] != '\0' && i<strlen(args); i++){
	mode[j] = args[i];
	j++;
      }
      mode[j] = '\0';
      i++;
      if (i <= strlen(args)){
	member = (char*)malloc(sizeof(char)*(MAX_NICK_LEN+1)); 
	for (; i <= strlen(args); i++){
	  member[k] = args[i];
	  k++;
	}
	member[k] = '\0';
      } 
    }
  } else {
    nick = (char*)malloc(sizeof(char)*MAX_NICK_LEN+1);
    mode = (char*)malloc(sizeof(char)*MODE_LEN*3);
    for (i=0; args[i] != ' ' && args[i] != '\0' ; i++){
      nick[i] = args[i];
    }
    nick[i] = '\0';
    i++;
    for (; /* args[i] != '\0' && args[i] != ' ' */ i <= strlen(args); i++){
      mode[j] = args[i];
      j++;
    }
  }
  
  pthread_mutex_t clock = get_lock_from_sock(socket);
  /* Handle User Modes */
  if (args[0] != '#'){
    if (strcmp(get_nick_from_sock(socket), nick)){
      err502 = (char*)malloc(sizeof(char)*(44+MAX_HOST_LEN+MAX_NICK_LEN));  
      sprintf(err502, ":%s 502 %s :Cannot change mode for other users\r\n", serverHost, nick);
      pthread_mutex_lock(&clock);
      
      if (send(socket,err502,strlen(err502),0) <= 0) {
	pthread_mutex_unlock(&clock);
	perror("Socket send() failed");
	close(socket);
	pthread_exit(NULL);
      }
      pthread_mutex_unlock(&clock);
      free(err502);
    } else {
      
      if (!strcmp(mode, "+o") | !strcmp(mode, "-a") | !strcmp(mode, "+a")){
	return 0;
      } else {
	if (!strcmp(mode, "-o")) {
	  if (deopp(socket)){
	    umreply = (char*)malloc(sizeof(char)*(11+MAX_NICK_LEN*2+MODE_LEN));
	    sprintf(umreply, ":%s MODE %s :%s\r\n", nick, nick, mode);
	    pthread_mutex_lock(&clock);
	
	    if (send(socket,umreply,strlen(umreply),0) <= 0){
	      pthread_mutex_unlock(&clock);
	      perror("Socket send() failed");
	      close(socket);
	      pthread_exit(NULL);
	    }
	  
	    pthread_mutex_unlock(&clock);
	    free(umreply);
	  }
	} else {
	  err501 = (char*)malloc(sizeof(char)*(27+MAX_HOST_LEN+MAX_NICK_LEN));
	  sprintf(err501, ":%s 501 %s :Unknown MODE flag\r\n", serverHost, nick);
	  pthread_mutex_lock(&clock);
     
	if (send(socket,err501,strlen(err501),0) <= 0){
	  perror("Socket send() failed");
	  close(socket);
	  pthread_exit(NULL);
	}
	pthread_mutex_unlock(&clock);
	free(err501);
	}
      }
    } 
  } else {
    /* Handle Channel Modes */
    
    pthread_mutex_lock(&lock);
    nick = get_nick_from_sock(socket);
    user = get_user_from_sock(socket);
    host = get_host_from_sock(socket);
    pthread_mutex_unlock(&lock);

    if (!is_already_chan(chan)){
      err403 = (char*)malloc(sizeof(char)*(26+MAX_CHAN_LEN+MAX_NICK_LEN+MAX_HOST_LEN));
      sprintf(err403, ":%s 403 %s %s :No such channel\r\n", serverHost, nick, chan);
      pthread_mutex_lock(&clock);
      
	if (send(socket,err403,strlen(err403),0) <= 0){
	  pthread_mutex_unlock(&clock);
	  perror("Socket send() failed");
	  close(socket);
	  pthread_exit(NULL);
	}
	pthread_mutex_unlock(&clock);
	free(err403);
    }
    
    if ((member != NULL) && (mode != NULL)){
      /* Specific to Member Status */

       if (!is_member_on_chan(member, get_users_on_channel(chan))){
	 err441 = (char*)malloc(sizeof(char)*(MAX_NICK_LEN+MAX_CHAN_LEN+MAX_HOST_LEN));
	 sprintf(err441, ":%s 441 %s %s %s :They aren't on that channel\r\n", serverHost, nick, member, chan);
	 pthread_mutex_lock(&clock);

	 if (send(socket,err441,strlen(err441),0) <= 0){
	   pthread_mutex_unlock(&clock);
	   perror("Socket send() failed");
	   close(socket);
	   pthread_exit(NULL);
	 }
	 pthread_mutex_unlock(&clock);
	 free(err441);
       } else {
	 if ((get_coper_from_sock(socket, chan) | get_oper_from_sock(socket)) && ((mode[1] == 'v') | (mode[1] == 'o'))){ 
	   msreply = (char*)malloc(sizeof(char)*(13+MAX_NICK_LEN*3+MAX_HOST_LEN+MAX_CHAN_LEN+MODE_LEN));
	   sprintf(msreply, ":%s!%s@%s MODE %s %s %s\r\n", nick, user, host, chan, mode, member);
	 }
       }
    }
    if (mode == NULL){ 
      /* first case in channel modes */
     
      modes = get_modes(chan);
      
      cmreply = (char*)malloc(sizeof(char)*(1+MAX_MSG_LEN));
      sprintf(cmreply, ":%s 324 %s %s +%s\r\n", serverHost, nick, chan, modes);  
	
      if (send(socket,cmreply,strlen(cmreply),0) <= 0){
	perror("Socket send() failed");
	close(socket);
	pthread_exit(NULL);
      }
    } else {  
	/* Handles second case of Channel Mode and Member Status*/ 
      if (((member == NULL) && (mode[1] != 'm') && (mode[1] != 't')) | ((member != NULL) && (mode[1] != 'v') &&  (mode[1] != 'o'))){
	err472 = (char*)malloc(sizeof(char)*(MAX_MSG_LEN+1));
	  sprintf(err472, ":%s 472 %s %c :is unknown mode char to me for %s\r\n",
		  serverHost, nick, mode[1], chan);

	  if (msreply != NULL){
	    free(msreply);
	    msreply = NULL;
	  }
	  pthread_mutex_lock(&clock);
	  
	  if (send(socket,err472,strlen(err472),0) <= 0){
	    pthread_mutex_unlock(&clock);
	    perror("Socket send() failed");
	    close(socket);
	    pthread_exit(NULL);
	  }
	  pthread_mutex_unlock(&clock);
	  free(err472);
      } else {       

	if (get_coper_from_sock(socket, chan) | get_oper_from_sock(socket)){

	  if (((mode[1] == 'm') && (((mode[0] == '-') && (change_chan_mode(chan, 1,0))) | 
				    ((mode[0] == '+') && change_chan_mode(chan, 1, 1))))
	      | ((mode[1] == 't') &&  (((mode[0] == '-') && (change_chan_mode(chan,2, 0))) | 
				       ((mode[0] == '+') && change_chan_mode(chan,2, 1))))){

	    cmreply = (char*)malloc(sizeof(char)*(MAX_MSG_LEN+1));
	    sprintf(cmreply, ":%s!%s@%s MODE %s %s\r\n", nick, user, host, chan, mode);
	   
	    if (cmreply != NULL){
	      send_to_channel(chan, cmreply);   
	    }
	  }
	  if (msreply != NULL){
	   
	    if (!strcmp(mode, "+o")){
	      make_copp(get_sock_from_nick(member),chan);
	    }
	    if (!strcmp(mode, "-o")){
	      decopp(get_sock_from_nick(member),chan);
	    }
	    if (!strcmp(mode, "+v")){
	      make_voice(get_sock_from_nick(member),chan);
	    }
	    if (!strcmp(mode, "-v")){
	      devoice(get_sock_from_nick(member),chan);
	    }
	    send_to_channel(chan,msreply);
	    free(msreply);
	  }
	} else {
	  err482 = (char*)malloc(sizeof(char)*(MAX_MSG_LEN+1));
	  sprintf(err482, ":%s 482 %s %s :You're not channel operator\r\n", serverHost, nick, chan);
	  pthread_mutex_lock(&clock);

	  if (send(socket,err482,strlen(err482),0) <= 0){
	    pthread_mutex_unlock(&clock);
	    perror("Socket send() failed");
	    close(socket);
	    pthread_exit(NULL);
	  }
	  pthread_mutex_lock(&clock);
	  free(err482);
	}
      }
    }
  }
  return 0;
}

/*############added handler for AWAY command############*/
/*WARNING: no locking implemented*/
int handle_AWAY(char* params, char* placeholder, int socket) {

  char* nickname = get_nick_from_sock(socket);

  char* sub = strstr(params, ":");
  char* msg;
  int i;
  if (sub != NULL) {
    msg = (char*)malloc(MAX_MSG_LEN+1);
    for (i=0; sub[i+1] != '\0' && i < MAX_MSG_LEN + 1; i++)
      msg[i] = sub[i+1];
    msg[i] = '\0';
  } else {
    for (i = 0; params[i] == ' '; i++)
      ;
    if (params[i] != '\0') {
      msg = (char*)malloc(MAX_MSG_LEN+1);
      int j;
      int max_endPrms = MAX_MSG_LEN + 1 - i;
      for (j=i; params[j] != ' ' && params[j] != '\0' && j < max_endPrms; j++)
	msg[j-i] = params[j];
      msg[j-i] = '\0';
    } else {
      msg = NULL;
    }
  }

  char* rplMsg = (char*)malloc(MAX_MSG_LEN+1);
  if (msg != NULL) {
    change_awayMsg(socket, msg);
    make_away(socket);
    sprintf(rplMsg, ":%s 306 %s :You have been marked as being away\r\n",
	    serverHost, nickname);
  } else {
    remove_awayMsg(socket);
    make_not_away(socket);
    sprintf(rplMsg, ":%s 305 %s :You are no longer marked as being away\r\n",
	    serverHost, nickname);
  }
  if (send(socket, rplMsg, strlen(rplMsg), 0) <= 0) {
    sock_is_gone(socket);
    if (msg != NULL)
      free(msg);
    free(rplMsg);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  if (msg != NULL)
    free(msg);
  free(rplMsg);
  return 0;
}

/*############added handler for LIST command############*/
/*WARNING: no locking implemented*/
int handle_LIST(char* params, char* placeholder, int socket) {
  char* chan2List = (char*)malloc(MAX_CHAN_LEN+1);
  char* nickname = get_nick_from_sock(socket);
  int i, j;
  for (i = 0; params[i] == ' '; i++)
    ;
  int maxPlusI = MAX_CHAN_LEN + i;
  for (j = i; params[j] != ' ' && params[j] != '\0' && j < maxPlusI; j++)
    chan2List[j-i] = params[j];
  chan2List[j-i] = '\0';

  if (strlen(chan2List)) {
    if (is_already_chan(chan2List)) {
      char* topic = get_topic(chan2List);
      char* msg = (char*)malloc(MAX_MSG_LEN+1);
      int numUsers = count_users_on_channel(chan2List);
      if (topic != NULL)
	sprintf(msg, ":%s 322 %s %s %d :%s\r\n",
		serverHost, nickname, chan2List, numUsers, topic);
      else
	sprintf(msg, ":%s 322 %s %s %d :\r\n",
		serverHost, nickname, chan2List, numUsers);
      if (send(socket, msg, strlen(msg), 0) <= 0) {
	sock_is_gone(socket);
	free(msg);
	free(chan2List);
	perror("Socket send() failed");
	close(socket);
	pthread_exit(NULL);
      }
      free(msg);
      free(chan2List);
    }
  } else {
    free(chan2List);

    channel* search = channels;
    int numUsers;
    while (search != NULL) {
      char* msg = (char*)malloc(MAX_MSG_LEN+1);
      char* topic = get_topic(search->name);
      numUsers = count_users_on_channel(search->name);
      if (topic != NULL)
	sprintf(msg, ":%s 322 %s %s %d :%s\r\n",
		serverHost, nickname, search->name, numUsers, topic);
      else
	sprintf(msg, ":%s 322 %s %s %d :\r\n",
		serverHost, nickname, search->name, numUsers);
      if (send(socket, msg, strlen(msg), 0) <= 0) {
	sock_is_gone(socket);
	free(msg);
	perror("Socket send() failed");
	close(socket);
	pthread_exit(NULL);
      }
      free(msg);
      search = search->next;
    }
  }
  char* endMsg = (char*)malloc(MAX_MSG_LEN+1);
  sprintf(endMsg, ":%s 323 %s :End of LIST\r\n", serverHost, nickname);
  if (send(socket, endMsg, strlen(endMsg), 0) <= 0) {
    sock_is_gone(socket);
    free(endMsg);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  free(endMsg);
  return 0;
}

/*############added handler for WHO command############*/
/*WARNING: no locking implemented*/
int handle_WHO (char* params, char* placeholder, int socket) {
  char* nickname = get_nick_from_sock(socket);
  int i;
  for (i = 0; params[i] == ' '; i++)
    ;
  if (params[i] != '\0' && params[i] != '*') {
    char* channame = (char*)malloc(MAX_CHAN_LEN+1);
    int j;
    for (j = i; params[j] != ' ' && params[j] != '\0' && j < MAX_CHAN_LEN+1; j++)
      channame[j-i] = params[j];
    channame[j-i] = '\0';
    chanUser* wholist =  get_users_on_channel(channame);
    char* whomsg;
    while (wholist != NULL) {
      //if (wholist->sock != socket) {
      whomsg = (char*)malloc(MAX_MSG_LEN+1);
      sprintf(whomsg, ":%s 352 %s %s %s %s %s %s %s%s%s%s :0 %s\r\n",
	      serverHost, nickname, channame, get_user_from_sock(wholist->sock),
	      get_host_from_sock(wholist->sock), serverHost,
	      get_nick_from_sock(wholist->sock), is_away(wholist->sock)?"G":"H",
	      get_oper_from_sock(wholist->sock) ? "*" : "", 
	      get_coper_from_sock(wholist->sock, channame) ? "@" : "",
	      get_voice_from_sock(wholist->sock, channame) ? "+" : "",
	      get_real_from_sock(wholist->sock));
      if (send(socket, whomsg, strlen(whomsg), 0) <= 0) {
	sock_is_gone(socket);
	free(whomsg);
	free(channame);
	perror("Socket send() failed");
	close(socket);
	pthread_exit(NULL);
	//}
	free(whomsg);
      }
      wholist = wholist->next;
    }
    char* endwho = (char*)malloc(MAX_MSG_LEN+1);
    sprintf(endwho, ":%s 315 %s %s :End of WHO list\r\n",
	    serverHost, nickname, channame);
    if (send(socket, endwho, strlen(endwho), 0) <= 0) {
      sock_is_gone(socket);
      free(endwho);
      free(channame);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    free(endwho);
    free(channame);
  } else {
    chanName* userChans = get_channels_on_user(socket);
    client* search = clients;
    char* whomsg;
    while (search != NULL) {
      if (!on_one_of_channels(userChans, search->sock)) {
	whomsg = (char*)malloc(MAX_MSG_LEN+1);
	sprintf(whomsg, ":%s 352 %s * %s %s %s %s %s%s :0 %s\r\n",
		serverHost, nickname, get_user_from_sock(search->sock),
		get_host_from_sock(search->sock), serverHost, 
		get_nick_from_sock(search->sock), is_away(search->sock)?"G":"H",
		get_oper_from_sock(search->sock) ? "*" : "",
		get_real_from_sock(search->sock));
	if (send(socket, whomsg, strlen(whomsg), 0) <= 0) {
	  sock_is_gone(socket);
	  free(whomsg);
	  perror("Socket send() failed");
	  close(socket);
	  pthread_exit(NULL);
	}
	free(whomsg);
      }
      search = search->next;
    }
    char* endwho = (char*)malloc(MAX_MSG_LEN+1);
    sprintf(endwho, ":%s 315 %s * :End of WHO list\r\n",
	    serverHost, nickname);
    if (send(socket, endwho, strlen(endwho), 0) <= 0) {
      sock_is_gone(socket);
      free(endwho);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
  }
  return 0;
}
