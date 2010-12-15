target = nitch_server
sources = main.c send_magic_packet.c
objects = $(sources:.c=.o)

VPATH = .
CC = gcc
CFLAGS = -W -Wall -O2 -I.
LDFLAGS = -pthread

all: $(target)

$(target): $(objects)
	$(CC) $(LDFLAGS) -o $@ $^ 

main.o: main.c send_magic_packet.h

send_magic_packet.o: send_magic_packet.c send_magic_packet.h

clean:
	rm -f $(target) $(objects)

.PHONY: all clean
