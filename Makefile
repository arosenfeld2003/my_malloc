CC = gcc
CFLAGS = -Wall -Wextra -Werror -g -std=c99

all: test_malloc

test_malloc: test_malloc.c my_malloc.c my_malloc.h
	$(CC) $(CFLAGS) test_malloc.c my_malloc.c -o test_malloc

clean:
	rm -rf *.dSYM
	rm -f test_malloc *.o

re: clean all

.PHONY: all clean re