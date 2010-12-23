target = nitch-notifier
source = main.c
object = $(source:.c=.o)

CC = gcc
CFLAGS = -W -Wall -O2

all: $(target)

$(target): $(object)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(target) $(object)

main.o: %.o: %.c

.PHONY: clean all
