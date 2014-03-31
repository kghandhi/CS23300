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
#include "get.h"
#include "addremove.h"
#include "globals.h"



/*############added initialization of global modes as 0 and chanName as null############*/
void add_client(char* nickname, char* username, char* hostname, char* realname, int socket) {
  pthread_mutex_lock(&lock);
  client* newFirst = (client*)malloc(sizeof(client));
  newFirst->user = strdup(username);
  newFirst->nick = strdup(nickname);
  newFirst->host = strdup(hostname);
  newFirst->real = strdup(realname);
  newFirst->sock = socket;
  newFirst->away = 0;
  newFirst->awayMsg = NULL;
  newFirst->oper = 0;
  pthread_mutex_init(&newFirst->lock,NULL);
  newFirst->chans = NULL;
  newFirst->next = clients;
  clients = newFirst;
  pthread_mutex_unlock(&lock);
  return;
}

/*############added freeing of user's chanName's############*/
/*fixed freeing in case when first list member is freed*/
/*fixed if condition in other case for when to free (changed clients->sock to cur->sock) */
void remove_client(int socket) {
  pthread_mutex_lock(&lock);
  if(clients==NULL)
    return;
  if(clients->sock==socket) {
    client* tmp = clients;
    clients = clients->next;
    free(tmp->user);
    free(tmp->nick);
    free(tmp->host);
    free(tmp->real);
    pthread_mutex_destroy(&tmp->lock);
    /*new freeing*/
    chanName* tmp2 = tmp->chans;
    chanName* tmp3;
    while (tmp2 != NULL) {
      tmp3 = tmp2;
      tmp2 = tmp2->next;
      free(tmp3->name);
      free(tmp3);
    }
    /*end new freeing*/
    free(tmp);
    pthread_mutex_unlock(&lock);
    return;
  }

  client* cur = clients->next;
  client* prev = clients;
  while (cur!=NULL && prev!=NULL) {
    /*change made to this if*/
    if(cur->sock==socket) {
      client* tmp = cur;
      prev->next = cur->next;
      free(tmp->user);
      free(tmp->nick);
      free(tmp->host);
      free(tmp->real);
      pthread_mutex_destroy(&tmp->lock);
      /*new freeing*/
      chanName* tmp2 = tmp->chans;
      chanName* tmp3;
      while (tmp2 != NULL) {
        tmp3 = tmp2;
        tmp2 = tmp2->next;
        free(tmp3->name);
        free(tmp3);
      }
      /*end new freeing*/
      free(tmp);
      pthread_mutex_unlock(&lock);
      return;
    }
    prev = cur;
    cur = cur->next;
  }
  pthread_mutex_unlock(&lock);
  return;
}


/*############added function for adding to global channel list############*/
void add_channel(char* name) {
  channel* newFirst = (channel*)malloc(sizeof(channel));
  newFirst->name = strdup(name);
  newFirst->topic = NULL;
  newFirst->modMode = 0;
  newFirst->topMode = 0;
  pthread_mutex_init(&newFirst->lock,NULL);
  newFirst->users = NULL;
  newFirst->next = channels;
  channels = newFirst;
  return;
}

/*############added function for removing from global channel list############*/
void remove_channel(char* channame) {
  if(channels==NULL)
    return;
  if(!strcmp(channels->name,channame)) {
    channel* tmp = channels;
    channels = channels->next;
    free(tmp->name);
    free(tmp->topic);
    pthread_mutex_destroy(&tmp->lock);
    chanUser* tmp2 = tmp->users;
    chanUser* tmp3;
    while (tmp2 != NULL) {
      tmp3 = tmp2;
      tmp2 = tmp2->next;
      free(tmp3->nick);
      free(tmp3);
    }
    free(tmp);
    return;
  }

  channel* cur = channels->next;
  channel* prev = channels;
  while (cur!=NULL && prev!=NULL) {
    if(!strcmp(cur->name, channame)) {
      channel* tmp = cur;
      prev->next = cur->next;
      free(tmp->name);
      free(tmp->topic);
      pthread_mutex_destroy(&tmp->lock);
      chanUser* tmp2 = tmp->users;
      chanUser* tmp3;
      while (tmp2 != NULL) {
        tmp3 = tmp2;
        tmp2 = tmp2->next;
        free(tmp3->nick);
        free(tmp3);
      }
      free(tmp);
      return;
    }
    prev = cur;
    cur = cur->next;
  }
  return;
}

/*############added function for adding user to channel in global list############*/
void add_chanUser(char* channame, char* nickname, int socket) {
  channel* search = channels;

  if (strlen(channame))
    while (search != NULL) {
      if (!strcmp(search->name, channame))
        if (!is_already_chanUser(search->users, socket)) {
          chanUser* newFirst = (chanUser*)malloc(sizeof(chanUser));
          newFirst->nick = strdup(nickname);
          newFirst->sock = socket;
          newFirst->coper = (search->users == NULL)? 1 : 0;
          newFirst->voice = 0;
          newFirst->next = search->users;
          search->users = newFirst;
	  return;
        }
      search = search->next;
    }
  return;
}

/*############added function for removing user from channel in global list############*/
void remove_chanUser(char* channame, int socket) {
  channel* search = channels;

  if (strlen(channame))
    while (search != NULL) {
      if (!strcmp(search->name, channame)) {
	if(search->users==NULL)
	  return;
	if(search->users->sock == socket) {
	  chanUser* tmp = search->users;
	  search->users = search->users->next;
	  free(tmp->nick);
	  free(tmp);
	  return;
	}

	chanUser* cur = search->users->next;
	chanUser* prev = search->users;
	while (cur!=NULL && prev!=NULL) {
	  if(cur->sock == socket) {
	    chanUser* tmp = cur;
	    prev->next = cur->next;
	    free(tmp->nick);
	    free(tmp);
	    return;
	  }
	  prev = cur;
	  cur = cur->next;
	}
      }
      search = search->next;
    }
  return;
}

/*############added function for adding channel to user in global list############*/
void add_chanName(int socket, char* channame) {
  client* search = clients;

  if (strlen(channame))
    while (search != NULL) {
      if (search->sock == socket)
        if (!is_already_chanName(search->chans, channame)) {
          chanName* newFirst = (chanName*)malloc(sizeof(chanName));
          newFirst->name = strdup(channame);
          newFirst->coper = (is_empty_channel(newFirst->name))? 1 : 0;
          newFirst->voice = 0;
          newFirst->next = search->chans;
          /*code is about to get messy, I'm not sure this works (as in add_chanUser)*/
          search->chans = newFirst;
	  return;
        }
      search = search->next;
    }
  return;
}

/*############added function for removing channel from user in global list############*/
void remove_chanName(int socket, char* channame) {
  client* search = clients;

  if (strlen(channame))
    while (search != NULL) {
      if (search->sock == socket) {
	if(search->chans==NULL)
	  return;
	if(!strcmp(search->chans->name,channame)) {
	  chanName* tmp = search->chans;
	  search->chans = search->chans->next;
	  free(tmp->name);
	  free(tmp);
	  return;
	}
	
	chanName* cur = search->chans->next;
	chanName* prev = search->chans;
	while (cur!=NULL && prev!=NULL) {
	  if(!strcmp(cur->name, channame)) {
	    chanName* tmp = cur;
	    prev->next = cur->next;
	    free(tmp->name);
	    free(tmp);
	    return;
	  }
	  prev = cur;
	  cur = cur->next;
	}
      }
      search = search->next;
    }
  return;
}

/*############added function for removing all influence of a socket on the server when it is no longer connected############*/
void sock_is_gone(int socket) {
  pthread_mutex_lock(&lock);
  if (is_registered(socket) && strlen(get_nick_from_sock(socket)) && strlen(get_user_from_sock(socket)))
    registeredClients -= 1;
  else
    unregisteredClients -=1;
  pthread_mutex_unlock(&lock);
  chanName* deadUser = get_channels_on_user(socket);
  while (deadUser != NULL) {
    remove_chanUser(deadUser->name, socket);
    deadUser = deadUser->next;
  }
  remove_client(socket);
}
