prefix = /usr/local

targets = $(main)
main = nitch-notifier
source = main.c
object = $(source:.c=.o)

targets += $(hook)
hook = 44_nitch-notifier
hook_source = print_hook.sh
pm_hook_dir = /etc/pm/sleep.d

CC = gcc
CFLAGS = -W -Wall -O2

all: $(targets)

$(main): $(object)
	$(CC) $(CFLAGS) -o $@ $^

$(hook): $(hook_source)
	./$< $(prefix) > $@

clean:
	rm -f $(targets) $(object)

install:
	install -d $(prefix)/bin
	install -m 0755 $(main) $(prefix)/bin
	install -m 0755 $(hook) $(pm_hook_dir)

uninstall:
	rm -f $(prefix)/bin/$(main)
	rm -f $(pm_hook_dir)/$(hook)

main.o: %.o: %.c

.PHONY: all clean install uninstall
