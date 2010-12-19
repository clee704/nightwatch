target = nitch-sleepd
sources = main.c agent.c agent_handler.c ui_handler.c packet_monitor.c \
  send_magic_packet.c resume_agent.c daemon.c network.c protocol.c \
  message_exchange.c
objects = $(sources:.c=.o)

VPATH = ../../commons/src
CC = gcc
CFLAGS = -W -Wall -O2 -D_REENTRANT -I../../commons/src
LDFLAGS = -lpthread

all: $(target)
$(target): $(objects)
	$(CC) $(LDFLAGS) -o $@ $^ 

clean:
	rm -f $(target) $(objects)

main.o: %.o: %.c \
  agent.h agent_handler.h ui_handler.h packet_monitor.h daemon.h

agent.o: %.o: %.c %.h \
  network.h

agent_handler.o: %.o: %.c %.h \
  agent.h network.h protocol.h message_exchange.h

ui_handler.o: %.o: %.c %.h \
  agent.h resume_agent.h network.h protocol.h message_exchange.h

packet_monitor.o: %.o: %.c %.h \
  agent.h

send_magic_packet.o: %.o: %.c %.h

resume_agent.o: %.o: %.c %.h \
  agent.h send_magic_packet.h

#
# common objects
#
daemon.o: %.o: %.c %.h

network.o: %.o: %.c %.h

protocol.o: %.o: %.c %.h

message_exchange.o: %.o: %.c %.h \
  network.h protocol.h

.PHONY: all clean
