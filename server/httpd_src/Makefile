target = nitch-httpd
sources = main.c ajax_handler.c daemon.c network.c protocol.c \
  message_exchange.c mongoose.c
objects = $(sources:.c=.o)

VPATH = ../../commons/src mongoose
CC = gcc
CFLAGS = -W -Wall -O2 -D_REENTRANT -I../../commons/src -Imongoose
LDFLAGS = -lpthread -ldl

all: $(target)

$(target): $(objects)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(target) $(objects)

#
# Objects
#
main.o: %.o: %.c \
  ajax_handler.h daemon.h logger.h protocol.h mongoose.h

ajax_handler.o: %.o: %.c %.h \
  logger.h network.h message_exchange.h mongoose.h

#
# Common objects
#
daemon.o: %.o: %.c %.h

network.o: %.o: %.c %.h

protocol.o: %.o: %.c %.h

message_exchange.o: %.o: %.c %.h \
  logger.h network.h protocol.h

#
# Library object
#
mongoose.o: %.o: %.c %.h

.PHONY: all clean
