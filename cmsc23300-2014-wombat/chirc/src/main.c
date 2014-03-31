/*
 *
 * CMSC 23300 / 33300 - Networks an Distributed Systems
 *
 * main() code for chirc project
 *
 */

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
#include "globals.h"
#include "welcome.h"
#include "login_handlers.h"
#include "msg_handlers.h"
#include "servermsg_handlers.h"
#include "quit_handler.h"
#include "channel_handlers.h"
#include "mode_handlers.h"
#include "send.h"
#include "change.h"
#include "modes.h"
#include "chaninfo.h"
#include "count.h"
#include "away.h"
#include "topic.h"
#include "get.h"
#include "is_already.h"
#include "addremove.h"

client* clients;
/*############added global channel list############*/
channel* channels;

char *port = "6700";
char *passwd = NULL;
char serverHost[MAX_HOST_LEN+1] = "bar.example.com";
time_t creationtime;
int unregisteredClients = 0;
int registeredClients = 0;

pthread_mutex_t lock; //global client lock
pthread_mutex_t lock2; //global channel lock
pthread_cond_t cv_setup_done;

struct workerArgs
{
  int socket;
  int setup_done;
};

void *accept_clients(void *args);
void *service_single_client(void *args);

typedef int (*handler_function)(char* a, char* b, int c);

struct handler_entry
{
  char *name;
  handler_function func;
};


#define HANDLER_ENTRY(NAME) { #NAME, handle_ ## NAME}

struct handler_entry handlers[] = {
  HANDLER_ENTRY(NICK),
  HANDLER_ENTRY(USER),
  HANDLER_ENTRY(PRIVMSG),
  HANDLER_ENTRY(NOTICE),
  HANDLER_ENTRY(PING),
  HANDLER_ENTRY(PONG),
  HANDLER_ENTRY(MOTD),
  HANDLER_ENTRY(LUSERS),
  HANDLER_ENTRY(WHOIS),
  HANDLER_ENTRY(QUIT),
  HANDLER_ENTRY(JOIN),
  HANDLER_ENTRY(PART),
  HANDLER_ENTRY(TOPIC),
  HANDLER_ENTRY(OPER),
  HANDLER_ENTRY(MODE),
  HANDLER_ENTRY(AWAY),
  HANDLER_ENTRY(NAMES),
  HANDLER_ENTRY(LIST),
  HANDLER_ENTRY(WHO),
};

int num_handlers = sizeof(handlers) / sizeof(struct handler_entry);




int main(int argc, char *argv[]) {
  int i;
  int opt;
  pthread_t server_thread;

  sigset_t new;

  while ((opt = getopt(argc, argv, "p:o:h")) != -1)
    switch (opt) {
    case 'p':
      port = strdup(optarg);
      break;
    case 'o':
      passwd = strdup(optarg);
      break;
    default:
      printf("ERROR: Unknown option -%c\n", opt);
      exit(-1);
    }
  if (!passwd) {
    fprintf(stderr, "ERROR: You must specify an operator password\n");
    exit(-1);
  }

  clients = NULL;
  gethostname(serverHost,MAX_HOST_LEN);
  char *tmp = gethostbyname(serverHost)->h_name;
  for (i = 0; *(tmp + i) != '\0'; i++)
    serverHost[i] = *(tmp + i);
  time(&creationtime);

  sigemptyset (&new);
  sigaddset(&new, SIGPIPE);
  if (pthread_sigmask(SIG_BLOCK, &new, NULL) != 0) {
    perror("Unable to mask SIGPIPE");
    exit(-1);
  }

  pthread_mutex_init(&lock, NULL);
  pthread_cond_init(&cv_setup_done, NULL);

  if (pthread_create(&server_thread, NULL, accept_clients, NULL) < 0) {
    perror("Could not create server thread");
    exit(-1);
  }

  pthread_join(server_thread, NULL);

  pthread_mutex_destroy(&lock);
  pthread_cond_destroy(&cv_setup_done);

  pthread_exit(NULL);
}

void *accept_clients(void *args) {
  int serverSocket;
  int clientSocket;
  pthread_t worker_thread;
  struct addrinfo hints, *res, *p;
  struct sockaddr_storage *clientAddr;
  socklen_t sinSize = sizeof(struct sockaddr_storage);
  struct workerArgs *wa;
  int yes = 1;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if (getaddrinfo(NULL, port, &hints, &res) != 0) {
    perror("getaddrinfo() failed");
    pthread_exit(NULL);
  }

  for(p = res;p != NULL; p = p->ai_next) {
    if ((serverSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("Could not open socket");
      continue;
    }

    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("Socket setsockopt() failed");
      close(serverSocket);
      continue;
    }

    if (bind(serverSocket, p->ai_addr, p->ai_addrlen) == -1) {
      perror("Socket bind() failed");
      close(serverSocket);
      continue;
    }

    if (listen(serverSocket, 5) == -1) {
      perror("Socket listen() failed");
      close(serverSocket);
      continue;
    }

    break;
  }
        
  freeaddrinfo(res);

  if (p == NULL) {
    fprintf(stderr, "Could not find a socket to bind to.\n");
    pthread_exit(NULL);
  }

  while (1) {
    clientAddr = malloc(sinSize);
    if ((clientSocket = accept(serverSocket, (struct sockaddr *) clientAddr, &sinSize)) == -1) {
      free(clientAddr);
      perror("Could not accept() connection");
      continue;
    }

    wa = malloc(sizeof(struct workerArgs));
    wa->socket = clientSocket;
    wa->setup_done = 0;

    pthread_mutex_lock(&lock);
    if (pthread_create(&worker_thread, NULL, service_single_client, wa) != 0) {
      perror("Could not create a worker thread");
      free(clientAddr);
      free(wa);
      close(clientSocket);
      close(serverSocket);
      pthread_exit(NULL);
    }

    while(!wa->setup_done)
      pthread_cond_wait(&cv_setup_done, &lock);
    pthread_mutex_unlock(&lock);
  }

  pthread_exit(NULL);
}

void *service_single_client(void *args) {
  struct workerArgs *wa;
  int socket, nbytes;
  char* input;
  char* newinput = (char*)malloc(2*MAX_MSG_LEN);
  char* nickname = (char*)malloc(MAX_NICK_LEN+1);
  char* username = (char*)malloc(MAX_USER_LEN+1);
  char* rn = "\r\n";
  char* command;
  int counter;
  char* cmdName;
  int rc;
  char* cmdParams;
  char* sub;
  char* unknown = "Unknown command";
  int firstTimeLogin = 1;
  char* hostname = (char*)malloc(MAX_HOST_LEN+1);
  char* realname = (char*)malloc(MAX_MSG_LEN+1);
  char* placeholder = NULL;
  struct sockaddr_in p;
  socklen_t p_len;
  int isRegistered = 0;
  int lenInput = MAX_MSG_LEN+1;
  int newlenInput;
  
  *nickname = '\0';
  *username = '\0';
  *hostname = '\0';
  *realname = '\0';
  newinput[0] = '\0';
  
  wa = (struct workerArgs*) args;
  socket = wa->socket;

  pthread_detach(pthread_self());

  wa->setup_done = 1;

  pthread_mutex_lock(&lock);
  pthread_cond_signal(&cv_setup_done);
  pthread_mutex_unlock(&lock);

  p_len = sizeof(p);
  getpeername(socket, (struct sockaddr*)&p, &p_len);
  strncat(hostname,gethostbyaddr((void*)&p.sin_addr, sizeof(struct in_addr), AF_INET)->h_name,MAX_HOST_LEN);

  pthread_mutex_lock(&lock);
  unregisteredClients += 1;
  pthread_mutex_unlock(&lock);

  while (1) {
    input = (char*)malloc(lenInput);
    command = (char*)malloc(MAX_MSG_LEN+1);
    cmdName = (char*)malloc(MAX_MSG_LEN+1);
    for (counter = 0; counter < strlen(newinput); counter++)
      *(input+counter) = *(newinput+counter);
    *(input + counter) = '\0';
    /* Receives packets from the client */
    while (1) {
      char buffer1[MAX_MSG_LEN];
      bzero(buffer1, MAX_MSG_LEN);
      if (strstr(input, rn) == NULL) {
        nbytes = recv(socket, buffer1, MAX_MSG_LEN, 0);
	if (nbytes == -1) {
	  sock_is_gone(socket);
	  free(input);
	  free(command);
	  free(cmdName);
	  free(newinput);
	  free(nickname);
	  free(username);
	  free(hostname);
	  free(realname);
	  perror("Socket recv() failed");
	  close(socket);
	  pthread_exit(NULL);
	}
	if (!nbytes) {
	  sock_is_gone(socket);
	  free(input);
	  free(command);
	  free(cmdName);
	  free(newinput);
	  free(nickname);
	  free(username);
	  free(realname);
	  perror("Server closed the connection");
	  close(socket);
	  pthread_exit(NULL);
	}
      }
      
      if ((newlenInput = strlen(input) + 1 + nbytes) > lenInput) {
	free(newinput);
	newinput = strdup(input);
	free(input);
	input = (char*)malloc(newlenInput+1);
	*input = '\0';
	strcat(input, newinput);
      }
      strncat(input, buffer1, nbytes);
      
      if ((sub = strstr(input, rn)) != NULL) {
	free(newinput);
        newinput = strdup(sub+2);
        for(counter = 0; *(input + counter) != '\r' &&
	      *(input + counter + 1) != '\n' && counter < MAX_MSG_LEN - 2; counter++) {
          *(command + counter) = *(input + counter);
        }
        *(command + counter) = '\0';
	free(input);
        break;
      }
    }

    int cmdSpaceCounter;
    for (cmdSpaceCounter = 0; *(command + cmdSpaceCounter) == ' '; cmdSpaceCounter++)
      ;

    for (counter = cmdSpaceCounter; *(command + counter) != ' ' && *(command + counter) != '\0'; counter++) {
      *(cmdName + counter - cmdSpaceCounter) = *(command + counter);
    }

    *(cmdName + counter - cmdSpaceCounter) = '\0';

    for(; *(command + counter) == ' '; counter++)
      ;
    cmdParams = command + counter;

    if (*cmdName != '\0') {
      for (counter = 0; counter == NICKCMD || counter == USERCMD; counter++) {
	if (!strcmp(handlers[counter].name, cmdName)) {	
	  switch (counter) {
	  case NICKCMD :
	    rc = handlers[counter].func(cmdParams, nickname, socket);
	    break;
	  case USERCMD :
	    rc = handlers[counter].func(cmdParams, username, socket);
	    if (!strlen(realname) || !isRegistered) {
	      free(realname);
	      realname = strdup(cmdParams);
	    }
	    break;
	  default :
	    break;
	  }
	  break;
	}
      }
      
      if(!isRegistered && counter != NICKCMD && counter !=USERCMD) {
	char* errorMsg = (char*)malloc(MAX_MSG_LEN+1);
	sprintf(errorMsg,":%s 451 * :You have not registered\r\n",serverHost);
	if (send(socket, errorMsg, strlen(errorMsg), 0) <= 0) {
	  sock_is_gone(socket);
	  free(command);
	  free(cmdName);
	  free(errorMsg);
	  free(nickname);
	  free(username);
	  free(hostname);
	  free(realname);
	  perror("Socket send() failed");
	  close(socket);
	  pthread_exit(NULL);
	}
	free(errorMsg);
      } else if (counter != NICKCMD && counter != USERCMD) {
	for (; counter < num_handlers; counter++) {
	  if (!strcmp(handlers[counter].name, cmdName)) {
	    rc = handlers[counter].func(cmdParams, placeholder, socket);
	    break;
	  }
	}

	if(counter == num_handlers) {
	  char* errorMsg = (char*)malloc(MAX_MSG_LEN+1);
	  sprintf(errorMsg,":%s 421 %s %s :%s\r\n",serverHost,nickname,cmdName,unknown);
	  if (send(socket, errorMsg, strlen(errorMsg), 0) <= 0) {
	    sock_is_gone(socket);
	    free(command);
	    free(cmdName);
	    free(errorMsg);
	    free(nickname);
	    free(username);
	    free(hostname);
	    free(realname);
	    perror("Socket send() failed");
	    close(socket);
	    pthread_exit(NULL);
	  }
	  free(errorMsg);
	}
      }

      if (rc)
	fprintf(stderr, "A Handler Function Failed, probably bad!\n");
    }
    
    free(command);
    free(cmdName);

    if (firstTimeLogin && strlen(nickname) && strlen(username)) {
      char* tmpuser;
     
      if (is_already_nick(nickname)) {
	pthread_mutex_lock(&lock);
        change_user_from_nick(nickname,username);
        change_real_from_nick(nickname,realname);
	pthread_mutex_unlock(&lock);
      } else if ((tmpuser = get_user_from_sock(socket))!=NULL && strlen(tmpuser)) {
	pthread_mutex_lock(&lock);
	change_nick_from_sock(socket,nickname);
	pthread_mutex_unlock(&lock);
      } else {
	add_client(nickname, username, hostname, realname, socket);
      }
      isRegistered = 1;
      
      pthread_mutex_lock(&lock);
      unregisteredClients -= 1; 
      registeredClients += 1; 
      pthread_mutex_unlock(&lock);
      
      welcome(nickname, username, hostname, socket);
      your_host(nickname,hostname,socket);
      created(nickname,socket);
      my_info(nickname,socket);
      handle_LUSERS(placeholder,placeholder,socket);
      handle_MOTD(placeholder,placeholder,socket);
      firstTimeLogin = 0;
    } else if (firstTimeLogin && (strlen(nickname) || strlen(username)) && !is_registered(socket)) {
      add_client(nickname, username, hostname, realname, socket);
    } else if (firstTimeLogin && strlen(nickname)) {
      pthread_mutex_lock(&lock);
      change_nick_from_sock(socket,nickname);
      pthread_mutex_unlock(&lock);
    } else if (firstTimeLogin && strlen(username)) {
      pthread_mutex_lock(&lock);
      change_user_from_sock(socket,username);
      change_real_from_sock(socket,realname);
      pthread_mutex_unlock(&lock);
    }
  }

  printf("(%d) service_single_client(): Disconnected.\n", socket);
  close(socket);
  pthread_exit(NULL);
}
