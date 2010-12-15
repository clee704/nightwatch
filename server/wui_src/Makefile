target = nitch-httpd
sources = main.c ajax.c daemon.c mongoose/mongoose.c
objects = $(sources:.c=.o)

VPATH = mongoose
CC = gcc
CFLAGS = -W -Wall -O2 -Imongoose
LDFLAGS = -ldl -pthread

all: $(target)

$(target): $(objects)
	$(CC) $(LDFLAGS) -o $@ $^ 

main.o: main.c ajax.h daemon.h mongoose.h

ajax.o: ajax.c mongoose.h ajax.h

daemon.o: daemon.c daemon.h

mongoose/mongoose.o: mongoose.c mongoose.h

clean:
	rm -f $(target) $(objects)

.PHONY: all clean
