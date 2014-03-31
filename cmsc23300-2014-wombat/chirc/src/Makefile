OBJS = main.o 
COBJS = $(OBJS) servermsg_handlers.c addremove.c away.c change.c chaninfo.c count.c get.c login_handlers.c msg_handlers.c quit_handler.c channel_handlers.c mode_handlers.c is_already.c modes.c send.c topic.c welcome.c 
DEPS = $(COBJS:.o=.d)
CC = gcc
CFLAGS = -I../../include -g3 -Wall -fpic -std=gnu99 -MMD -MP -DDEBUG -DMUTEX
BIN = ../chirc
LDLIBS = -pthread

all: $(BIN)
	
$(BIN): $(COBJS)
	$(CC) $(LDFLAGS) $(LDLIBS) $(COBJS) -o $(BIN)
	
%.d: %.c

clean:
	-rm -f $(OBJS) $(BIN) *.d
