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
#include "welcome.h"
#include "get.h"
#include "globals.h"

void welcome(char* nickname, char* username, char* hostname, int socket) {
  char* msg = (char*)malloc(MAX_MSG_LEN+1);

  msg[0] = '\0';
  sprintf(msg,":%s 001 %s :Welcome to the Internet Relay Network %s!%s@%s\r\n",
          serverHost,nickname,nickname,username,hostname);
  /*only send Welcome message if weve gotten both NICK and USER*/
  pthread_mutex_t clock = get_lock_from_sock(socket);
  pthread_mutex_lock(&clock);
  if (send(socket, msg, strlen(msg), 0) <= 0) {
    sock_is_gone(socket);
    free(msg);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  pthread_mutex_unlock(&clock);
  free(msg);
}

void your_host(char* nickname, char* hostname, int socket) {
  char* msg = (char*)malloc(MAX_MSG_LEN+1);

  msg[0] = '\0';
  sprintf(msg, ":%s 002 %s :Your host is %s, running version chirc-0.3.2\r\n",
          serverHost,nickname,hostname);
  pthread_mutex_t clock = get_lock_from_sock(socket);
  pthread_mutex_lock(&clock);
  if (send(socket, msg, strlen(msg), 0) <= 0) {
    pthread_mutex_unlock(&clock);
    sock_is_gone(socket);
    free(msg);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  pthread_mutex_unlock(&clock);
  free(msg);
}

void created(char* nickname, int socket) {
  char* msg = (char*)malloc(MAX_MSG_LEN+1);
  char buffer[80];
  struct tm* timeinfo;
  
  timeinfo = localtime(&creationtime);

  strftime(buffer,80,"%F %X",timeinfo);

  msg[0] = '\0';
  sprintf(msg, ":%s 003 %s :This server was created %s\r\n",
          serverHost, nickname, buffer);
  pthread_mutex_t clock = get_lock_from_sock(socket);
  pthread_mutex_lock(&clock);
  if (send(socket, msg, strlen(msg), 0) <= 0) {
    pthread_mutex_unlock(&clock);
    sock_is_gone(socket);
    free(msg);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  pthread_mutex_unlock(&clock);
  free(msg);
}

void my_info(char* nickname, int socket) {
  char* msg = (char*)malloc(MAX_MSG_LEN+1);

  msg[0] = '\0';
  sprintf(msg, ":%s 004 %s %s chirc-0.3.2 ao mtov\r\n",
          serverHost,nickname,serverHost);
  pthread_mutex_t clock = get_lock_from_sock(socket);
  pthread_mutex_lock(&clock);
  if (send(socket, msg, strlen(msg), 0) <= 0) {
    pthread_mutex_unlock(&clock);
    sock_is_gone(socket);
    free(msg);
    perror("Socket send() failed");
    close(socket);
    pthread_exit(NULL);
  }
  pthread_mutex_unlock(&clock);
  free(msg);
}
