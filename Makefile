CC=gcc
CFLAGS=-g -I.

sources = $(wildcard *.c)
objects = $(sources:.c=.o)

fsm: $(objects)
	$(CC) -o $@ $^

.PHONY: clean
clean:
	rm -f $(objects) fsm
