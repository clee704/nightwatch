.SUFFIXES : .c .o

CC = gcc


all: reporter main


reporter: agent_reporter.c
	gcc -o 01_nitch_$@ agent_reporter.c `pkg-config --libs --cflags dbus-1`

get_system_info: get_system_info.c get_system_info.h
	gcc -c -o get_system_info.o get_system.info.c

main : main.c main.h get_system_info.o
	gcc -o $@ main.c `pkg-config --libs --cflags dbus-1`
	rm get_system_info.o

clean:
	rm main 01_nitch_reporter
