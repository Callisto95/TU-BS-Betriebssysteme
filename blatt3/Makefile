-include ../common_abgabe.mk
CFLAGS  = -std=c11 -pedantic -D_XOPEN_SOURCE=700 -Wall -Werror -g
LDFLAGS =
CC		= gcc
.PHONY: all doc clean

all: crawl

doc:
	$(RM) -r html
	doxygen

crawl: crawl.o argumentParser.o
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^

crawl.o: crawl.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $^

clean:
	$(RM) crawl *.o

testArgs: testArgs.o argumentParser.o

test:
	python3 tests/unittest.py -t tests
