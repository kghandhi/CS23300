#define NLOOPS 1000000
#define MAX_MSG_LEN 512
#define MAX_NICK_LEN 9
#define MAX_USER_LEN 9
#define MAX_HOST_LEN 63
#define MAX_CHAN_LEN 50
#define MAX_PWD_LEN 300
#define MODE_LEN 2
#define NICKCMD 0
#define USERCMD 1
#define PRIVMSGCMD 2
#define NOTICECMD 3
#define PINGCMD 4
#define PONGCMD 5
#define MOTDCMD 6
#define LUSERSCMD 7
#define WHOISCMD 8
#define QUITCMD 9
/*############added project 1c commands############*/
#define JOINCMD 10
#define PARTCMD 11
#define TOPICCMD 12
#define OPERCMD 13
#define MODECMD 14
#define AWAYCMD 15
#define NAMESCMD 16
#define LISTCMD 17
#define WHOCMD 18

#ifndef GLOBALS_H
#define GLOBALS_H

/*############added chanName struct############*/
/*we want users to have a list of channels they're on*/
/*but if we use the channel struct, the existence of*/
/*the users part could lead to an escalating series of*/
/*having to free like an entire servers worth of information*/
/*to remove one user; plus we can put user channel specific*/
/*modes here*/
typedef struct chanName {
  char* name;
  int coper;
  int voice;
  struct chanName* next;
} chanName;

/*############added chanUser struct############*/
/*this exists for similar reasons to the chanName struct*/
/*we won't need all the information from the client struct*/
/*when checking who the users are on a channel and this might*/
/*make freeing easier; currently this lets us determine which*/
/*nicks and which sockets are on a channel, so we can uniquely*/
/*identify users; we might want to remove coper and voice later*/
/*if we don't need them both in chanName and chanUser for our*/
/*functions but I'm including them for now in case they're useful*/
typedef struct chanUser {
  char* nick;
  int sock;
  int coper;
  int voice;
  struct chanUser* next;
} chanUser;

/*############updated struct to include user's channels and global modes############*/
typedef struct client {
  char* user;
  char* nick;
  char* host;
  char* real;
  int sock;
  int away;
  char* awayMsg;
  int oper;
  pthread_mutex_t lock;
  chanName* chans;
  struct client* next;
} client;

/*############added channel struct############*/
typedef struct channel {
  char* name;
  char* topic;
  int modMode;
  int topMode;
  pthread_mutex_t lock;
  chanUser* users;
  struct channel* next;
} channel;


extern client* clients;
/*############added global channel list############*/
/*will probably need to make sure it has a lock so that it works right*/
extern channel* channels;

extern char *passwd;
extern char serverHost[MAX_HOST_LEN+1];
extern time_t creationtime;
extern int unregisteredClients;
extern int registeredClients;

extern pthread_mutex_t lock;  //global client lock
extern pthread_mutex_t lock2; //global channel lock

#endif
