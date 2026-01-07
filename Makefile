CC = gcc
CFLAGS = -Wall -Wextra -Werror -g -std=c99

all: test_malloc

test_malloc: test_malloc.c my_malloc.c my_malloc.h
	$(CC) $(CFLAGS) test_malloc.c my_malloc.c -o test_malloc

# Build a custom program using my_malloc
# Usage: make my_malloc_test_program (requires my_malloc_test_program.c to exist)
my_malloc_test_program: my_malloc_test_program.c my_malloc.c my_malloc.h
	$(CC) $(CFLAGS) my_malloc_test_program.c my_malloc.c -o my_malloc_test_program

# Run the test suite
test: test_malloc
	./test_malloc

clean:
	rm -rf *.dSYM
	rm -f test_malloc my_malloc_test_program *.o

re: clean all

.PHONY: all clean re test