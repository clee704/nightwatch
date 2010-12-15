target = nitch_server_httpd
sources = main.c ajax.c mongoose/mongoose.c
objects = $(sources:.c=.o)

VPATH = mongoose
CC = gcc
CFLAGS = -W -Wall -O2 -Imongoose
LDFLAGS = -ldl -pthread

all: $(target)

$(target): $(objects)
	$(CC) $(LDFLAGS) -o $@ $^ 

main.o: main.c mongoose.h ajax.h

ajax.o: ajax.c mongoose.h ajax.h

mongoose/mongoose.o: mongoose.c mongoose.h

clean:
	rm -f $(target) $(objects)

.PHONY: all clean
