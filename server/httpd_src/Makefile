target = nitch-httpd
sources = main.c mongoose.c daemon.c protocol.c
objects = $(sources:.c=.o)

VPATH = mongoose ../../commons/src
CC = gcc
CFLAGS = -W -Wall -O2 -Imongoose -I../../commons/src
LDFLAGS = -ldl -pthread

all: $(target)

$(target): $(objects)
	$(CC) $(LDFLAGS) -o $@ $^ 

main.o: main.c daemon.h protocol.h mongoose.h

mongoose.o: mongoose.c mongoose.h

daemon.o: daemon.c daemon.h

protocol.o: protocol.c protocol.h

clean:
	rm -f $(target) $(objects)

.PHONY: all clean
