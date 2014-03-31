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
#include "quit_handler.h"
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
