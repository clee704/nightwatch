target = nitch_server_httpd
sources = main.c ajax.c mongoose/mongoose.c
objects = $(sources:.c=.o)

CC = gcc
CFLAGS = -W -Wall -O2 -Imongoose
LDFLAGS = -ldl -pthread

all: $(target)

$(target): $(objects)
	$(CC) $(LDFLAGS) -o $@ $^ 

clean:
	rm -f $(target) $(objects)

.PHONY: all clean
