#Mengting Nan
#EECS 570 
#Winter 2014 
#Project 1a 
#Setting gcc 4.7.0

PATH := /usr/um/gcc-4.7.0/bin:$(PATH)

LD_LIBRARY_PATH := /usr/um/gcc-4.7.0/lib64

LD_RUN_PATH := /usr/um/gcc-4.7.0/lib64

CC = g++

#Recommended compiler and linker flags

CFLAGS = -c -pedantic -std=c++11 -Wall -Wextra -g -O2

LFLAGS = -pedantic -Wall -Wextra -ldl -pthread


OBJS = prefix_sum.o

PROG = exe

#Prevents automatic build rules, which can lead to unexpected behavior.

default:

	make real -r -R

real: $(PROG)

$(PROG): $(OBJS)

	$(CC) $(LFLAGS) $(OBJS) -o $(PROG)

prefix_sum.o: prefix_sum.cc

	$(CC) $(CFLAGS) prefix_sum.cc

clean:
	rm -f *.o
	rm -f *exe

run:
	time ./exe 1 3 
