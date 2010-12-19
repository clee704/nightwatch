target = nitch-notifier
source = reporter.c
object = $(source:.c=.o)

CC = gcc
CFLAGS = -W -Wall -O2

all: $(target)

$(target): $(object)
	$(CC) -o $@ $^

$(object) : $(source)

clean:
	rm -r $(target) $(object)

.PHONY: clean all
