#Setting gcc 4.7.0
PATH := /usr/um/gcc-4.7.0/bin:$(PATH)
LD_LIBRARY_PATH := /usr/um/gcc-4.7.0/lib64
LD_RUN_PATH := /usr/um/gcc-4.7.0/lib64
CC = g++

parallel_sum:
	g++ main.cpp prefix_sum.h -pthread -std=c++11 -Wall -o parallel_sum
clean:
	rm -f parallel_sum
