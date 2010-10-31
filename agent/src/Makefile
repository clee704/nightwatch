CC = gcc
CFLAGS = -g -O0

all: nitch_agent

nitch_agent: main.c
	$(CC) $(CFLAGS) -o $@ $+

clean:
	-rm nitch_agent

check: all
	./nitch_agent | grep "Hello from .*nitch_agent!"
	@echo "*** ALL TESTS PASSED ***"

install:
	install -d $(DESTDIR)$(bindir)
	install -m 0755 nitch_agent $(DESTDIR)$(bindir)

uninstall:
	-rm $(DESTDIR)$(bindir)/nitch_agent

.PHONY: all clean check install uninstall
