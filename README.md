# Welcome to My Malloc
***

## Task
Create a homegrown implementation of the malloc family functions in order to allocate memory:
- my_malloc
- my_free
- my_calloc
- my_realloc

## Description
A custom memory allocator using a **doubly-linked list** with **block splitting** and **coalescing** to reduce fragmentation.

**Key features:**
- In-band metadata (32-byte header per block)
- First-fit allocation strategy
- O(1) coalescing using prev pointers
- mmap-based memory acquisition
- 22 comprehensive tests 

## Installation
```bash
# Build the project
make

# Or rebuild from scratch
make re

# Run test suite
make test
```

## Usage
```c
#include "my_malloc.h"

// Allocate memory
void *ptr = my_malloc(100);

// Zero-initialized allocation
int *arr = my_calloc(10, sizeof(int));

// Resize allocation
arr = my_realloc(arr, 20 * sizeof(int));

// Free memory
my_free(ptr);
my_free(arr);
```

**Compile with my_malloc:**
```bash
gcc -Wall -Wextra -Werror -g -std=c99 your_program.c my_malloc.c -o your_program
```

**Run tests:**
```bash
./test_malloc
```

See [DESIGN.md](DESIGN.md) for detailed architecture and implementation.

### The Core Team


<span><i>Made at <a href='https://qwasar.io'>Qwasar SV -- Software Engineering School</a></i></span>
<span><img alt='Qwasar SV -- Software Engineering School's Logo' src='https://storage.googleapis.com/qwasar-public/qwasar-logo_50x50.png' width='20px' /></span>
