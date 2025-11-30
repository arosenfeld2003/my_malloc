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

static block_header_t * head = NULL;            // head of linked list

void *my_malloc(size_t size) {
    if (size == 0) return NULL;                 // allocating a 0-sized chunk returns NULL

    // TODO:
    // 1. Search linked list for free block
    // 2. If not found, mmap new block -> see https://man7.org/linux/man-pages/man2/mmap.2.html
    // 3. Mark block as allocated
    // 4. Return pointer after header

    return NULL;
}

void my_free(void *ptr) {
    if (ptr == NULL) return;

    // TODO:
    // 1. Get header -> (block_header_t *)ptr - 1
    // 2. Mark as free
    // 3. Coalesce ->
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