target = nitch-sleepd
sources = main.c agent.c agent_handler.c ui_handler.c packet_monitor.c \
  send_magic_packet.c resume_agent.c daemon.c network.c protocol.c \
  message_exchange.c poison.c
objects = $(sources:.c=.o)

VPATH = ../../commons/src
CC = gcc
CFLAGS = -W -Wall -O2 -D_REENTRANT -I../../commons/src
LDFLAGS = -lpthread -lpcap

all: $(target)
$(target): $(objects)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(target) $(objects)

main.o: %.o: %.c \
  agent.h agent_handler.h ui_handler.h packet_monitor.h daemon.h logger.h

agent.o: %.o: %.c %.h \
  logger.h network.h

agent_handler.o: %.o: %.c %.h \
  agent.h logger.h network.h protocol.h message_exchange.h poison.h

ui_handler.o: %.o: %.c %.h \
  agent.h resume_agent.h logger.h network.h protocol.h message_exchange.h

packet_monitor.o: %.o: %.c %.h \
  agent.h logger.h

send_magic_packet.o: %.o: %.c %.h

resume_agent.o: %.o: %.c %.h \
  agent.h send_magic_packet.h logger.h

#
# Common objects
#
daemon.o: %.o: %.c %.h

network.o: %.o: %.c %.h

protocol.o: %.o: %.c %.h

message_exchange.o: %.o: %.c %.h \
  logger.h network.h protocol.h

poison.o: %.o: %.c %.h

.PHONY: all clean
