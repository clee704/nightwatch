.SUFFIXES : .c .o

CC = gcc


all: reporter main


reporter: agent_reporter.c
	gcc -o $@ agent_reporter.c `pkg-config --libs --cflags dbus-1`

main : main.c
	gcc -o $@ main.c `pkg-config --libs --cflags dbus-1`
