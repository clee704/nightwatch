target = nitch-agentd
sources = main.c get_system_info.c network.c daemon.c protocol.c poison.c
objects = $(sources:.c=.o)

CC = gcc
CFLAGS = -W -Wall -O2 -I../../commons/src
LDFLAGS = -lpthread
VPATH = ../../commons/src

all: $(target)

$(target): $(objects)
	$(CC) $(LDFLAGS) -o $@ $^

main.o : %.o: %.c\
	daemon.h network.h get_system_info.h poison.h

get_system_info.o : %.o: %.c

clean:
		rm -f $(target) $(objects)

