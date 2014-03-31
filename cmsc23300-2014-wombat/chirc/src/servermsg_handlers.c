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
#include "servermsg_handlers.h"
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

  pthread_mutex_t slock =  get_lock_from_sock(socket);
  pthread_mutex_lock(&slock);
  if (!f){
    sprintf(errmsg, ":%s 422 %s :MOTD File is missing\r\n", serverHost,nick);
    if (send(socket, errmsg, strlen(errmsg),0) <= 0) {
      pthread_mutex_unlock(&slock);
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
      pthread_mutex_unlock(&slock);
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
	pthread_mutex_unlock(&slock);
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
      pthread_mutex_unlock(&slock);
      sock_is_gone(socket);
      free(msg3);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    free(msg3);
    fclose(f);
  }
  pthread_mutex_unlock(&slock);
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

  pthread_mutex_t slock = get_lock_from_sock(socket);
  pthread_mutex_lock(&slock);
  if (send(socket,msg1,strlen(msg1),0) <= 0) {
    pthread_mutex_unlock(&slock);
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
    pthread_mutex_unlock(&slock);
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
    pthread_mutex_unlock(&slock);
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
    pthread_mutex_unlock(&slock);
    sock_is_gone(socket);
    free(msg4);
    free(msg5);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  free(msg4);
  if (send(socket,msg5,strlen(msg5),0) <= 0) {
    pthread_mutex_unlock(&slock);
    sock_is_gone(socket);
    free(msg5);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  pthread_mutex_unlock(&slock);
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
  
  int awaysts = is_away(search->sock);
  char* awaymsg = get_awayMsg(search->sock);
  int opersts = get_oper_from_sock(search->sock);
  pthread_mutex_unlock(&lock);

  pthread_mutex_lock(&lock2);
  chanName* nickChans = get_channels_on_user(search->sock);
  char* msg4;
  if (nickChans != NULL) {
    msg4 = (char*)malloc(MAX_MSG_LEN+1);
    sprintf(msg4, ":%s 319 %s %s :", serverHost, nick, mask);
    chanName* searchN = nickChans;
    while(searchN != NULL) {
      if (get_coper_from_sock(search->sock, nickChans->name)) {
	strcat(msg4, "@");
      } else if (get_voice_from_sock(search->sock, nickChans->name)) {
	strcat(msg4, "+");
      }
      strcat(msg4, nickChans->name);
      strcat(msg4, " ");
      searchN = searchN->next;
    }
    strcat(msg4, "\r\n");
  }
  pthread_mutex_unlock(&lock2);

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
    
  if (nickChans != NULL) {
    if (send(socket,msg4,strlen(msg4),0) <= 0) {
      pthread_mutex_unlock(&clock);
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
   
  if (awaysts) {
    char* msg5 = (char*)malloc(MAX_MSG_LEN+1);
    sprintf(msg5, ":%s 301 %s %s :%s\r\n", serverHost, nick, mask, awaymsg);
    if (send(socket,msg5,strlen(msg5),0) <= 0) {
      pthread_mutex_unlock(&clock);
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

  if (opersts) {
    char* msg6 = (char*)malloc(MAX_MSG_LEN+1);
    sprintf(msg6, ":%s 313 %s %s :is an IRC operator\r\n", serverHost, nick, mask);
    if (send(socket,msg6,strlen(msg6),0) <= 0) {
      pthread_mutex_unlock(&clock);
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

  if (send(socket,msg3,strlen(msg3),0) <= 0) {
    pthread_mutex_unlock(&clock);
    sock_is_gone(socket);
    free(mask);
    free(msg3);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  pthread_mutex_unlock(&clock);
  free(mask);
  free(msg3);
  return 0;
}

/*############added handler for NAMES command############*/
int handle_NAMES(char* params, char* placeholder, int socket) {
  char* nickname = get_nick_from_sock(socket);
  int i;
  
  pthread_mutex_lock(&lock);
  pthread_mutex_t slock = get_lock_from_sock(socket);
  pthread_mutex_unlock(&lock);

  for (i = 0; params[i] == ' '; i++)
    ;
  if (params[i] == '\0') {
    
    pthread_mutex_lock(&lock2);
    channel* chans = channels;
    while (chans != NULL) {
      char* chan2Names = chans->name;
      chanUser* search = get_users_on_channel(chan2Names);
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
      pthread_mutex_unlock(&lock2);
      
      char* msg = (char*)malloc(MAX_MSG_LEN+1);
      sprintf(msg, ":%s 353 %s = %s :%s\r\n",
	      serverHost, nickname, chan2Names, userlist);
      pthread_mutex_lock(&slock);
      if (send(socket, msg, strlen(msg), 0) <= 0) {
	pthread_mutex_unlock(&slock);
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
      pthread_mutex_unlock(&slock);
    }

    pthread_mutex_lock(&lock);
    client* peeps = clients; 
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
    pthread_mutex_unlock(&lock);

    if(strlen(channelless)) {
      char* msg2 = (char*)malloc(MAX_MSG_LEN+1);
      sprintf(msg2, ":%s 353 %s * * :%s\r\n",
	      serverHost, nickname, channelless);
      pthread_mutex_lock(&slock);
      if (send(socket, msg2, strlen(msg2), 0) <= 0) {
	pthread_mutex_lock(&slock);
	sock_is_gone(socket);
	free(msg2);
	free(channelless);
	perror("Socket send() failed");
	close(socket);
	pthread_exit(NULL);
      }
      pthread_mutex_unlock(&slock);
      free(msg2);
    }
    free(channelless);
    char* msg3 = (char*)malloc(MAX_MSG_LEN+1);
    sprintf(msg3, ":%s 366 %s * :End of NAMES list\r\n",
	    serverHost, nickname);
    pthread_mutex_lock(&slock);
    if (send(socket, msg3, strlen(msg3), 0) <= 0) {
      pthread_mutex_unlock(&slock);
      sock_is_gone(socket);
      free(msg3);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    pthread_mutex_unlock(&slock);
    free(msg3);
  } else {
    char* chan2Names = (char*)malloc(MAX_CHAN_LEN+1);
    for(; params[i] != ' ' && params[i] != '\0'; i++)
      chan2Names[i] = params[i];
    chan2Names[i] = '\0';
   

    pthread_mutex_lock(&lock2);
    chanUser* search = get_users_on_channel(chan2Names);
   
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
    pthread_mutex_unlock(&lock2);

    if (strlen(userlist)) {
      char* msg = (char*)malloc(MAX_MSG_LEN+1);
      sprintf(msg, ":%s 353 %s = %s :%s\r\n",
	      serverHost, nickname, chan2Names, userlist);
      
      pthread_mutex_lock(&slock);
      if (send(socket, msg, strlen(msg), 0) <= 0) {
	pthread_mutex_unlock(&slock);
	sock_is_gone(socket);
	free(msg);
	free(chan2Names);
	free(userlist);
	perror("Socket send() failed");
	close(socket);
	pthread_exit(NULL);
      }
      pthread_mutex_unlock(&slock);
      free(msg);
    }
    free(userlist);
    char* msg2 = (char*)malloc(MAX_MSG_LEN+1);
    sprintf(msg2, ":%s 366 %s %s :End of NAMES list\r\n", serverHost, nickname,chan2Names);
    
    pthread_mutex_lock(&slock);
    if (send(socket, msg2, strlen(msg2), 0) <= 0) {
      pthread_mutex_lock(&slock);
      sock_is_gone(socket);
      free(msg2);
      free(chan2Names);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    pthread_mutex_unlock(&slock);
    free(msg2);
    free(chan2Names);
  }
  return 0;
}

/*############added handler for LIST command############*/
int handle_LIST(char* params, char* placeholder, int socket) {
  char* chan2List = (char*)malloc(MAX_CHAN_LEN+1);
  int i, j;

  pthread_mutex_lock(&lock);
  char* nickname = get_nick_from_sock(socket);
  pthread_mutex_t slock = get_lock_from_sock(socket);
  pthread_mutex_unlock(&lock);

  for (i = 0; params[i] == ' '; i++)
    ;
  int maxPlusI = MAX_CHAN_LEN + i;
  
  for (j = i; params[j] != ' ' && params[j] != '\0' && j < maxPlusI; j++)
    chan2List[j-i] = params[j];
  chan2List[j-i] = '\0';

  if (strlen(chan2List)) {
    pthread_mutex_lock(&lock2);
    int is_chan = is_already_chan(chan2List);
    pthread_mutex_t chlock;
    if (is_chan){
      chlock = get_lock_from_chan(chan2List);
    }
    pthread_mutex_unlock(&lock2);

    if (is_chan) {
      pthread_mutex_lock(&chlock);
      char* topic = get_topic(chan2List);
      char* msg = (char*)malloc(MAX_MSG_LEN+1);
      int numUsers = count_users_on_channel(chan2List);
      pthread_mutex_unlock(&chlock);
      if (topic != NULL)
	sprintf(msg, ":%s 322 %s %s %d :%s\r\n",
		serverHost, nickname, chan2List, numUsers, topic);
      else
	sprintf(msg, ":%s 322 %s %s %d :\r\n",
		serverHost, nickname, chan2List, numUsers);
      pthread_mutex_lock(&slock);
      if (send(socket, msg, strlen(msg), 0) <= 0) {
	pthread_mutex_unlock(&slock);
	sock_is_gone(socket);
	free(msg);
	free(chan2List);
	perror("Socket send() failed");
	close(socket);
	pthread_exit(NULL);
      }
      pthread_mutex_unlock(&slock);
      free(msg);
      free(chan2List);
    }
  } else {
    free(chan2List);


    pthread_mutex_lock(&lock2);
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
    pthread_mutex_unlock(&lock2); 
  }
  char* endMsg = (char*)malloc(MAX_MSG_LEN+1);
  sprintf(endMsg, ":%s 323 %s :End of LIST\r\n", serverHost, nickname);
  pthread_mutex_lock(&slock);
  if (send(socket, endMsg, strlen(endMsg), 0) <= 0) {
    pthread_mutex_unlock(&slock);
    sock_is_gone(socket);
    free(endMsg);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  pthread_mutex_unlock(&slock);
  free(endMsg);
  return 0;
}

/*############added handler for WHO command############*/
int handle_WHO (char* params, char* placeholder, int socket) {
  int i;

  pthread_mutex_lock(&lock);
  char* nickname = get_nick_from_sock(socket);
  pthread_mutex_t slock = get_lock_from_sock(socket);
  pthread_mutex_unlock(&lock);
  
  for (i = 0; params[i] == ' '; i++)
    ;
  if (params[i] != '\0' && params[i] != '*') {
    char* channame = (char*)malloc(MAX_CHAN_LEN+1);
    int j;
    for (j = i; params[j] != ' ' && params[j] != '\0' && j < MAX_CHAN_LEN+1; j++)
      channame[j-i] = params[j];
    channame[j-i] = '\0';
    
    pthread_mutex_lock(&lock2);
    pthread_mutex_t chlock = get_lock_from_chan(channame);
    pthread_mutex_unlock(&lock2);

    pthread_mutex_lock(&chlock);
    chanUser* wholist =  get_users_on_channel(channame);
    pthread_mutex_unlock(&chlock);
    
    char* whomsg;
    while (wholist != NULL) {
      whomsg = (char*)malloc(MAX_MSG_LEN+1);
      pthread_mutex_lock(&lock);
      sprintf(whomsg, ":%s 352 %s %s %s %s %s %s %s%s%s%s :0 %s\r\n",
	      serverHost, nickname, channame, get_user_from_sock(wholist->sock),
	      get_host_from_sock(wholist->sock), serverHost,
	      get_nick_from_sock(wholist->sock), is_away(wholist->sock)?"G":"H",
	      get_oper_from_sock(wholist->sock) ? "*" : "", 
	      get_coper_from_sock(wholist->sock, channame) ? "@" : "",
	      get_voice_from_sock(wholist->sock, channame) ? "+" : "",
	      get_real_from_sock(wholist->sock));
      pthread_mutex_unlock(&lock);
      pthread_mutex_lock(&slock);
      if (send(socket, whomsg, strlen(whomsg), 0) <= 0) {
	pthread_mutex_unlock(&slock);
	sock_is_gone(socket);
	free(whomsg);
	free(channame);
	perror("Socket send() failed");
	close(socket);
	pthread_exit(NULL);
	free(whomsg);
      }
      wholist = wholist->next;
      pthread_mutex_unlock(&slock);
    }
    char* endwho = (char*)malloc(MAX_MSG_LEN+1);
    sprintf(endwho, ":%s 315 %s %s :End of WHO list\r\n",
	    serverHost, nickname, channame);
    pthread_mutex_lock(&slock);
    if (send(socket, endwho, strlen(endwho), 0) <= 0) {
      pthread_mutex_unlock(&slock);
      sock_is_gone(socket);
      free(endwho);
      free(channame);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    pthread_mutex_unlock(&slock);
    free(endwho);
    free(channame);
  } else {
    pthread_mutex_lock(&lock2);
    chanName* userChans = get_channels_on_user(socket);
    pthread_mutex_unlock(&lock2);
    
    pthread_mutex_lock(&lock);
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
	pthread_mutex_unlock(&lock);
	pthread_mutex_lock(&slock);
	if (send(socket, whomsg, strlen(whomsg), 0) <= 0) {
	  pthread_mutex_unlock(&slock);
	  sock_is_gone(socket);
	  free(whomsg);
	  perror("Socket send() failed");
	  close(socket);
	  pthread_exit(NULL);
	}
	pthread_mutex_unlock(&slock);
	free(whomsg);
	pthread_mutex_lock(&lock);
      }
      search = search->next;
    }
    pthread_mutex_unlock(&lock);
    char* endwho = (char*)malloc(MAX_MSG_LEN+1);
    sprintf(endwho, ":%s 315 %s * :End of WHO list\r\n",
	    serverHost, nickname);
    pthread_mutex_lock(&slock);
    if (send(socket, endwho, strlen(endwho), 0) <= 0) {
      pthread_mutex_unlock(&slock);
      sock_is_gone(socket);
      free(endwho);
      perror("Socket send() failed");
      close(socket);
      pthread_exit(NULL);
    }
    pthread_mutex_unlock(&slock);
  }
  return 0;
}
