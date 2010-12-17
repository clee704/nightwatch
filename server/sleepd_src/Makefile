target = nitch-sleepd
sources = main.c send_magic_packet.c daemon.c network.c protocol.c
objects = $(sources:.c=.o)

VPATH = ../../commons/src
CC = gcc
CFLAGS = -W -Wall -O2 -I../../commons/src
LDFLAGS = -pthread

all: $(target)

$(target): $(objects)
	$(CC) $(LDFLAGS) -o $@ $^ 

main.o: main.c send_magic_packet.h agent.h daemon.h network.h protocol.h

send_magic_packet.o: send_magic_packet.c send_magic_packet.h

daemon.o: daemon.c daemon.h

network.o: network.c network.h

protocol.o: protocol.c protocol.h

clean:
	rm -f $(target) $(objects)

.PHONY: all clean
