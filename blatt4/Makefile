-include ../common_abgabe.mk
CFLAGS  = -std=c11 -pedantic -D_XOPEN_SOURCE=700 -Wall -Werror -g
CC      = gcc
RM      = rm -f
.PHONY: clean doc test

all: clash

clash: clash.o plist.o plist_walklist.o
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $^

clean:
	$(RM) clash *.o

test:
	python3 tests/unittest.py -t tests/
