CC     = gcc
CFLAGS = -Wall -Wextra -std=c99 -pedantic -O2 -D_POSIX_C_SOURCE=200809L

all: lexico

lexico: main.o lexer.o
	$(CC) $(CFLAGS) -o $@ main.o lexer.o

main.o: main.c lexer.h
lexer.o: lexer.c lexer.h

test: lexico
	./lexico exemplo.ssl

clean:
	rm -f *.o lexico

.PHONY: all test clean
