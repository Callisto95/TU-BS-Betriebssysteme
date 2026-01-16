-include ../common_abgabe.mk
CFLAGS=-std=c11 -Wall -Werror -pedantic -D_XOPEN_SOURCE=700 -g
CC=gcc
RM      = rm -f

.PHONY: all clean

all: concat

clean:
	$(RM) -f concat

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: tests
	python3 tests/unittest.py -t tests/
