#include "my_malloc.h"
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

/*
 * ARCHITECTURE: In-band metadata (header before user pointer)
 *
 * Block Header Structure:
 * +------------------+
 * | size             | 8 bytes
 * | is_free          | 1 byte
 * | padding          | 7 bytes (alignment)
 * | next             | 8 bytes
 * +------------------+ <- Total: 24 bytes
 * | user data...     | <- my_malloc returns pointer here
 * +------------------+
 */

typedef struct block_header {
    size_t size;                                // user data, no header
    char is_free;                               // 1 -> free, 0 -> allocated
    char padding[7];                            // 8-byte alignment
    struct block_header *next;                  // Linked list -> all blocks
} block_header_t;

#define HEADER_SIZE (sizeof(block_header_t))
#define MMAP_BLOCK_SIZE 4096                    // standard 32 and 64-bit linux

static block_header_t *head = NULL;             // head of linked list
static int mmap_count = 0;                      // debug: track mmap calls

void *my_malloc(size_t size) {
    if (size == 0) return NULL;                 // allocating a 0-sized chunk returns NULL

    // 1. Search linked list for free block that fits
    block_header_t *current = head;
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            // Found a free block that fits
            current->is_free = 0;               // mark as allocated
            return (void *)(current + 1);       // return pointer after header
        }
        current = current->next;
    }

    // 2. No suitable block found - mmap a new one
    size_t alloc_size = HEADER_SIZE + size;
    if (alloc_size < MMAP_BLOCK_SIZE) {
        alloc_size = MMAP_BLOCK_SIZE;           // minimum allocation
    }

    void *ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (ptr == MAP_FAILED) {
        return NULL;                            // mmap failed
    }

    mmap_count++;
    write(STDERR_FILENO, "DEBUG: mmap called\n", 19);

    // 3. Initialize block header
    block_header_t *block = (block_header_t *)ptr;
    block->size = alloc_size - HEADER_SIZE;     // user data size
    block->is_free = 0;                         // allocated
    block->next = head;                         // insert at head
    head = block;

    // 4. Return pointer after header
    return (void *)(block + 1);
}

void my_free(void *ptr) {
    if (ptr == NULL) return;

    // 1. Get header (pointer arithmetic: go back one block_header_t)
    block_header_t *block = (block_header_t *)ptr - 1;

    // 2. Mark as free
    block->is_free = 1;

    // 3. No coalescing yet (Week 2 - keeping it simple)
    // We'll add coalescing in Week 3
}

// stub
void *my_calloc(size_t nmemb, size_t size) {
    (void)nmemb;
    (void)size;
    return NULL;
}

// stub
void *my_realloc(void *ptr, size_t size) {
    (void)ptr;
    (void)size;
    return NULL;
}