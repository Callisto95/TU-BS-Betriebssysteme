-include ../common_abgabe.mk
CFLAGS  = -std=c11 -pedantic -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -Wall -Werror -fpie -pthread -g -ggdb
LDFLAGS = -pie -pthread
CC      = gcc
RM      = rm -f
.PHONY: all clean doc test

all: ticker

ticker: ticker.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $^

clean:
	$(RM) ticker *.o

test:
	python3 tests/unittest.py -t tests/
