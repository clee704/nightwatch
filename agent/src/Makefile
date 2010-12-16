.SUFFIXES : .c .o

CC = gcc


all: main reporter

reporter: reporter.c
	gcc -o nitch_reporter reporter.c

get_system_info: get_system_info.c get_system_info.h
	gcc -c -o get_system_info.o get_system.info.c

main : main.c main.h get_system_info.o
	gcc -o $@ main.c get_system_info.o `pkg-config --libs --cflags dbus-1`
	rm get_system_info.o

clean:
	rm main nitch_reporter
