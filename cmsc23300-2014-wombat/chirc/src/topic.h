#ifndef TOPIC_H
#define TOPIC_H

int has_topic(char* channame);

char* get_topic(char* channame);

void change_topic(char* channame, char* topic);

void remove_topic(char* channame);

#endif
