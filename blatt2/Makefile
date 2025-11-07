CC = gcc
CFLAGS = -std=c11 -pedantic -D_XOPEN_SOURCE=700 -Wall -Werror -g

halde: main.o halde.o
	${CC} -o $@ $^

halde.o: halde.c halde.h
	${CC} ${CFLAGS} -c -o $@ $<

main.o: main.c halde.h
	${CC} ${CFLAGS} -c -o $@ $<

clean:
	rm -f halde halde.o main.o halde-ref.o

test:
	python3 tests/unittest.py -t tests/

.PHONY: clean test
