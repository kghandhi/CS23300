#ifndef SEND_H
#define SEND_H

void send_to_channel(char* channame, char* msg);

void send_to_rest_of_channel(char* channame, char* msg, int sendSock);

void send2_to_rest_of_channel(char* channame, char* msg1, char* msg2, int sendSock);

#endif
