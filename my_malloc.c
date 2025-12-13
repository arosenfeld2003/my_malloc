#include "my_malloc.h"
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>

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

 * pointer arithmetic:
  - block points to address 0x1000 (start of header)
    - block + 1 means: 0x1000 + (1 × sizeof(block_header_t))..
    - IMPORTANT: c pointer arithmetic automatically multiplies by type size!
    - Since sizeof(block_header_t) = 24 bytes, this gives: 0x1000 + 24 = 0x1018
    - So block + 1 points to the byte immediately after the header

 * Address 0x1000:  +------------------+
 *                  | size (8 bytes)   |
 *                  +------------------+
 *                  | is_free (1 byte) |
 *                  +------------------+
 *                  | padding (7 bytes)|
 *                  +------------------+
 *                  | next (8 bytes)   |
 * Address 0x1018:   +------------------+ <-- block + 1 points HERE
 *                  | user data...     |
 *                  | user data...     |
 *                  +------------------+
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
// https://man7.org/linux/man-pages/man2/mmap.2.html
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

    // 1. Get header (pointer arithmetic, in reverse: go back one block_header_t)
    block_header_t *block = (block_header_t *)ptr - 1;

    // 2. Mark as free
    block->is_free = 1;

    // TODO: coalescing
}

/*
 * size_t nmemb - the number of elements in memory allocation request
 * e.g. "an array of N things, each of size S."
 * this extra param provides security by preventing buffer overflows!
 * see: https://stackoverflow.com/questions/9901366/try-to-buffer-overflow-value-allocated-by-malloc
*/
void *my_calloc(size_t nmemb, size_t size) {
    // Edge case: if either is 0, return NULL
    if (nmemb == 0 || size == 0) {
        return NULL;
    }

    // Check for overflow: nmemb * size must not overflow size_t
    // If nmemb * size > SIZE_MAX, then nmemb > SIZE_MAX / size
    if (nmemb > SIZE_MAX / size) {
        return NULL;  // Would overflow
    }

    size_t total_size = nmemb * size;

    // Reuse my_malloc to allocate memory
    void *ptr = my_malloc(total_size);
    if (ptr == NULL) {
        return NULL;  // Allocation failed
    }

    // Zero-initialize the memory
    memset(ptr, 0, total_size);

    return ptr;
}

// stub
void *my_realloc(void *ptr, size_t size) {
    (void)ptr;
    (void)size;
    return NULL;
}