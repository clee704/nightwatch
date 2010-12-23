target = nitch-agentd
sources = main.c get_system_info.c daemon.c network.c protocol.c poison.c
objects = $(sources:.c=.o)

VPATH = ../../commons/src
CC = gcc
CFLAGS = -W -Wall -O2 -D_REENTRANT -I../../commons/src
LDFLAGS = -lpthread

all: $(target)

$(target): $(objects)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(target) $(objects)

main.o: %.o: %.c \
  get_system_info.h daemon.h network.h poison.h

get_system_info.o: %.o: %.c

.PHONY: all clean
