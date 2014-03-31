#ifndef MODES_H
#define MODES_H

char* get_modes(char* chan);

int deopp(int socket);

int make_opp(int socket);

int decopp(int socket, char* chan);

int make_copp(int socket, char* chan);

int  make_voice(int socket, char* chan);

int devoice(int socket, char* chan);

int get_voice_from_sock(int socket, char* chan);

int get_modMode(char *chan);

int get_topMode(char *chan);

int change_chan_mode(char* chan, int mode, int on_off);

int get_oper_from_sock(int socket);

int get_coper_from_sock(int socket, char* channel);

#endif
