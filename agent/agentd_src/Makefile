target = nitch-agentd
sources = main.c get_system_info.c
objects = $(sources:.c=.o)

CC = gcc
CFLAGS = -W -Wall -O2 `pkg-config --libs --cflags dbus-1`
LDFLAGS = -pthread

all: $(target)

$(target): $(objects)
	$(CC) $(LDFLAGS) -o $@ $^

main.o : %.o: %.c\
	get_system_info.h

get_system_info.o : %.o: %.c

clean:
		rm -f $(target) $(objects)

