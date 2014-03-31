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
#include "mode_handlers.h"
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
  pthread_mutex_t clock = get_lock_from_sock(socket);
  pthread_mutex_unlock(&lock);
  
  if (!password | !user){ 
    return 0;
  }

 
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
    for (; i <= strlen(args); i++){
      mode[j] = args[i];
      j++;
    }
  }

  pthread_mutex_lock(&lock);
  pthread_mutex_t clock = get_lock_from_sock(socket);
  char* cmpnick = get_nick_from_sock(socket);
  pthread_mutex_unlock(&lock);

  /* Handle User Modes */
  if (args[0] != '#'){
    if (strcmp(cmpnick, nick)){
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
	  pthread_mutex_lock(&lock2);
	  int did_we_deopp = deopp(socket);
	  pthread_mutex_unlock(&lock2);
	  if (did_we_deopp){
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
	  pthread_mutex_unlock(&clock);
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

    pthread_mutex_lock(&lock2);
    int is_chan = is_already_chan(chan);
    pthread_mutex_t chlock = get_lock_from_chan(chan);
    pthread_mutex_unlock(&lock2);


    if (!is_chan){
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

      pthread_mutex_lock(&lock2);
      int memsts = is_member_on_chan(member, get_users_on_channel(chan));
      pthread_mutex_unlock(&lock2);
       if (!memsts){
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
	 pthread_mutex_lock(&lock2);
	 int get_coper = get_coper_from_sock(socket, chan);
	 int get_oper = get_oper_from_sock(socket);
	 pthread_mutex_unlock(&lock2);

	 if ((get_coper | get_oper) && ((mode[1] == 'v') | (mode[1] == 'o'))){ 
	   msreply = (char*)malloc(sizeof(char)*(13+MAX_NICK_LEN*3+MAX_HOST_LEN+MAX_CHAN_LEN+MODE_LEN));
	   sprintf(msreply, ":%s!%s@%s MODE %s %s %s\r\n", nick, user, host, chan, mode, member);
	 }
       }
    }
    if (mode == NULL){ 
      /* first case in channel modes */
      pthread_mutex_lock(&chlock);
      modes = get_modes(chan);
      pthread_mutex_unlock(&chlock);
	
      cmreply = (char*)malloc(sizeof(char)*(1+MAX_MSG_LEN));
      sprintf(cmreply, ":%s 324 %s %s +%s\r\n", serverHost, nick, chan, modes);  
      pthread_mutex_lock(&clock);
      if (send(socket,cmreply,strlen(cmreply),0) <= 0){
	pthread_mutex_unlock(&clock);
	perror("Socket send() failed");
	close(socket);
	pthread_exit(NULL);
      }
      pthread_mutex_unlock(&clock);
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
	      printf("SENT");
	    }
	  }
	  if (msreply != NULL){
	   
	    if (!strcmp(mode, "+o")){
	      pthread_mutex_lock(&chlock);
	      make_copp(get_sock_from_nick(member),chan);
	      pthread_mutex_unlock(&chlock);
	    }
	    if (!strcmp(mode, "-o")){
	      pthread_mutex_lock(&chlock);
	      decopp(get_sock_from_nick(member),chan);
	      pthread_mutex_unlock(&chlock);
	    }
	    if (!strcmp(mode, "+v")){
	      pthread_mutex_lock(&chlock);
	      make_voice(get_sock_from_nick(member),chan);
	      pthread_mutex_unlock(&chlock);
	    }
	    if (!strcmp(mode, "-v")){
	      pthread_mutex_lock(&chlock);
	      devoice(get_sock_from_nick(member),chan);
	      pthread_mutex_unlock(&chlock);
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
int handle_AWAY(char* params, char* placeholder, int socket) {
  char* sub = strstr(params, ":");
  char* msg;
  int i;

  pthread_mutex_lock(&lock);
  char* nickname = get_nick_from_sock(socket);
  pthread_mutex_t slock = get_lock_from_sock(socket);
  pthread_mutex_unlock(&lock);
 
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
    pthread_mutex_lock(&slock);
    change_awayMsg(socket, msg);
    make_away(socket);
    pthread_mutex_unlock(&slock);

    sprintf(rplMsg, ":%s 306 %s :You have been marked as being away\r\n",
	    serverHost, nickname);
  } else {
    pthread_mutex_lock(&slock);
    remove_awayMsg(socket);
    make_not_away(socket);
    pthread_mutex_unlock(&slock);
    sprintf(rplMsg, ":%s 305 %s :You are no longer marked as being away\r\n",
	    serverHost, nickname);
  }
  pthread_mutex_lock(&slock);
  if (send(socket, rplMsg, strlen(rplMsg), 0) <= 0) {
    pthread_mutex_unlock(&slock);
    sock_is_gone(socket);
    if (msg != NULL)
      free(msg);
    free(rplMsg);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  pthread_mutex_unlock(&slock);
  if (msg != NULL)
    free(msg);
  free(rplMsg);
  return 0;
}
