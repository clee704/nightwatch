.SUFFIXES: .c .o

CC = gcc

all : agentd notifier

agentd:
	cd agentd_src; $(MAKE)

notifier:
	cd notifier_src; $(MAKE)

install:
	cp 44_nitch-notifier /etc/pm/sleep.d/;
	cp notifier_src/nitch-notifier /usr/local/bin/

clean:
	cd agentd_src; $(MAKE) clean;
	cd notifier_src; $(MAKE) clean;

