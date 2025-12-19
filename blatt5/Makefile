-include ../common_abgabe.mk
CC=gcc

CFLAGS=-std=c11 -Wall -Werror -pedantic -D_XOPEN_SOURCE=700 -pthread -g

LDFLAGS=-pthread

.PHONY: all clean

all: patric

clean:
	rm -f triangle.o patric.o patric

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

patric: patric.o triangle.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
	

patric.o:   patric.c triangle.h
triangle.o: triangle.c triangle.h

test:
	python3 tests/unittest.py -t tests/
