#Setting gcc 4.7.0

PATH := /usr/um/gcc-4.7.0/bin:$(PATH)

LD_LIBRARY_PATH := /usr/um/gcc-4.7.0/lib64

LD_RUN_PATH := /usr/um/gcc-4.7.0/lib64

CC = g++

#Recommended compiler and linker flags

CFLAGS = -c -pedantic -std=c++11 -Wall -Wextra -g 

LFLAGS = -pedantic -Wall -Wextra -ldl -pthread

#OBJS = disk.o Foo.o
OBJS = thread.o disk.o libcpu.a

PROG = exe

#Prevents automatic build rules, which can lead to unexpected behavior.

default:

	make real -r -R

real: $(PROG)

$(PROG): $(OBJS)

	$(CC) $(LFLAGS) $(OBJS) -o $(PROG)

disk.o: disk.cc thread.h

	$(CC) $(CFLAGS) disk.cc

#Foo.o: Foo.cpp Foo.h FooImpl.h
#
#        $(CC) $(CFLAGS) Foo.cpp

clean:
	cp thread.o thread.txt
	rm -f *.o
	mv thread.txt thread.o

real_clean:

	cp thread.o thread.txt
	rm -f *.o
	mv thread.txt thread.o
	rm -f *exe
