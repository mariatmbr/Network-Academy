CC=gcc
CFLAGS=-Wall -Wextra -Wpedantic -m32 -std=c11
PUBL=publications

.PHONY: build clean

build: publications.o

publications.o: publications.c publications.h hashtable.h
	$(CC) $(CFLAGS) publications.c -c

clean:
	rm -f *.o *.h.gch
