CC = gcc
CFLAGS = -g -O0

all: nitch_server

nitch_server: main.c
	$(CC) $(CFLAGS) -o $@ $+

clean:
	-rm nitch_server

check: all
	./nitch_server | grep "Hello from .*nitch_server!"
	@echo "*** ALL TESTS PASSED ***"

install:
	install -d $(DESTDIR)$(bindir)
	install -m 0755 nitch_server $(DESTDIR)$(bindir)

uninstall:
	-rm $(DESTDIR)$(bindir)/nitch_server

.PHONY: all clean check install uninstall
