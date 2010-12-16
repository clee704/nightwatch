target = nitch-sleepd
sources = main.c send_magic_packet.c common/daemon.c common/protocol.c
objects = $(sources:.c=.o)

VPATH = common
CC = gcc
CFLAGS = -W -Wall -O2 -Icommon
LDFLAGS = -pthread

all: $(target)

$(target): $(objects)
	$(CC) $(LDFLAGS) -o $@ $^ 

main.o: main.c agent.h send_magic_packet.h daemon.h protocol.h

send_magic_packet.o: send_magic_packet.c send_magic_packet.h

common/daemon.o: daemon.c daemon.h

common/protocol.o: protocol.c protocol.h

clean:
	rm -f $(target) $(objects)

.PHONY: all clean
