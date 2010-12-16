target = nitch-httpd
sources = main.c mongoose/mongoose.c common/daemon.c common/protocol.c
objects = $(sources:.c=.o)

VPATH = mongoose common
CC = gcc
CFLAGS = -W -Wall -O2 -Imongoose -Icommon
LDFLAGS = -ldl -pthread

all: $(target)

$(target): $(objects)
	$(CC) $(LDFLAGS) -o $@ $^ 

main.o: main.c daemon.h protocol.h mongoose.h

mongoose/mongoose.o: mongoose.c mongoose.h

common/daemon.o: daemon.c daemon.h

common/protocol.o: protocol.c protocol.h

clean:
	rm -f $(target) $(objects)

.PHONY: all clean
