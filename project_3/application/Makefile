#Makefile to compile sys_app
CC = gcc
CFLAGS = -Wall -Wextra
OBJECTS = mem_app

all : $(OBJECTS)
	echo "Compilation Terminated. Executable file is: mem_app"
	echo "In order to run the executable: "./mem_app""

syscall_wrapper.o: syscall_wrapper.c syscall_wrapper.h
	echo "Compiling syscall_wrapper.c: "
	$(CC) $(CFLAGS) -c syscall_wrapper.c

libsys_mem.a: syscall_wrapper.o
	echo "Archiving *.o files into static library libsys_mem.a"
	ar -rcs libsys_mem.a syscall_wrapper.o

mem_app: mem_app.c libsys_mem.a
	echo "Finally compiling source code mem_app"
	$(CC) $(CFLAGS) mem_app.c -o mem_app -L. -lsys_mem

clean:
	rm syscall_wrapper.o libsys_mem.a $(OBJECTS)

