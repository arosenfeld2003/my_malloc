#include "my_malloc.h"
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>

/*
 * ARCHITECTURE: In-band metadata (header before user pointer)
 * STRUCTURE: Doubly-linked list for O(1) coalescing
 *
 * Block Header Structure:
 * +------------------+
 * | size             | 8 bytes  (user data size, excludes header)
 * | is_free          | 1 byte   (0 = allocated, 1 = free)
 * | padding          | 7 bytes  (for 8-byte alignment)
 * | next             | 8 bytes  (pointer to next block in list)
 * | prev             | 8 bytes  (pointer to previous block in list)
 * +------------------+ <- Total: 32 bytes
 * | user data...     | <- my_malloc returns pointer here
 * +------------------+

 * Pointer arithmetic:
 *  - block points to address 0x1000 (start of header)
 *  - block + 1 means: 0x1000 + (1 × sizeof(block_header_t))
 *  - IMPORTANT: C pointer arithmetic automatically multiplies by type size!
 *  - Since sizeof(block_header_t) = 32 bytes, this gives: 0x1000 + 32 = 0x1020
 *  - So block + 1 points to the byte immediately after the header
 *
 * Memory layout example:
 * Address 0x1000:  +------------------+
 *                  | size (8 bytes)   |
 *                  +------------------+
 *                  | is_free (1 byte) |
 *                  +------------------+
 *                  | padding (7 bytes)|
 *                  +------------------+
 *                  | next (8 bytes)   |
 *                  +------------------+
 *                  | prev (8 bytes)   |
 * Address 0x1020:  +------------------+ <-- block + 1 points HERE
 *                  | user data...     |
 *                  | user data...     |
 *                  +------------------+
 *
 * Linked list structure:
 *    NULL <- [Block 3] <-> [Block 2] <-> [Block 1] -> NULL
 *                                           ^
 *                                         head
 */

typedef struct block_header {
    size_t size;                                // user data, no header
    char is_free;                               // 1 -> free, 0 -> allocated
    char padding[7];                            // 8-byte alignment
    struct block_header *next;                  // Next block in linked list
    struct block_header *prev;                  // Previous block in linked list (for O(1) coalescing)
} block_header_t;

#define HEADER_SIZE (sizeof(block_header_t))
#define MMAP_BLOCK_SIZE 4096                    // standard 32 and 64-bit linux

static block_header_t *head = NULL;             // head of linked list
// https://man7.org/linux/man-pages/man2/mmap.2.html
static int mmap_count = 0;                      // debug: track mmap calls

// Debug function to check mmap count
int get_mmap_count(void) {
    return mmap_count;
}

void *my_malloc(size_t size) {
    if (size == 0) return NULL;                 // allocating a 0-sized chunk returns NULL

    // 1. Search linked list for free block that fits
    block_header_t *current = head;
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            // Found a free block that fits

            // Check if block is large enough to split
            // Only split if remainder would be useful (>= HEADER_SIZE + minimum useful size)
            size_t min_split_size = HEADER_SIZE + 32;  // Minimum 32 bytes user data in remainder

            if (current->size >= size + min_split_size) {
                // Split the block
                size_t original_size = current->size;

                // Current block becomes the allocated portion
                current->size = size;
                current->is_free = 0;

                // Create new block for the remainder
                // Calculate address: current + header + size
                block_header_t *remainder = (block_header_t *)((char *)(current + 1) + size);
                remainder->size = original_size - size - HEADER_SIZE;
                remainder->is_free = 1;
                remainder->next = current->next;
                remainder->prev = current;

                // Update next block's prev pointer (if it exists)
                if (remainder->next != NULL) {
                    remainder->next->prev = remainder;
                }

                // Insert remainder into linked list after current
                current->next = remainder;

                return (void *)(current + 1);
            } else {
                // Block isn't large enough to split, use whole block
                current->is_free = 0;               // mark as allocated
                return (void *)(current + 1);       // return pointer after header
            }
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
    block->prev = NULL;                         // new head has no prev

    // Update old head's prev pointer (if it exists)
    if (head != NULL) {
        head->prev = block;
    }

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

    // 3. Coalesce with next block (if free and physically adjacent)
    block_header_t *next = block->next;
    if (next != NULL && next->is_free) {
        // Check if blocks are physically adjacent in memory
        void *expected_next_addr = (char *)(block + 1) + block->size;
        if ((void *)next == expected_next_addr) {
            // Merge next into block
            block->size += HEADER_SIZE + next->size;
            block->next = next->next;

            // Update the next->next block's prev pointer (if it exists)
            if (next->next != NULL) {
                next->next->prev = block;
            }
        }
    }

    // 4. Coalesce with previous block (if free and physically adjacent)
    block_header_t *prev = block->prev;
    if (prev != NULL && prev->is_free) {
        // Check if blocks are physically adjacent in memory
        void *expected_block_addr = (char *)(prev + 1) + prev->size;
        if ((void *)block == expected_block_addr) {
            // Merge block into prev
            prev->size += HEADER_SIZE + block->size;
            prev->next = block->next;

            // Update the block->next's prev pointer (if it exists)
            if (block->next != NULL) {
                block->next->prev = prev;
            }
            // Note: block is now absorbed into prev, so we're done
        }
    }
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

void *my_realloc(void *ptr, size_t size) {
    // Edge case 1: ptr is NULL -> behave like malloc
    if (ptr == NULL) {
        return my_malloc(size);
    }

    // Edge case 2: size is 0 -> behave like free and return NULL
    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    // Get the current block header
    block_header_t *block = (block_header_t *)ptr - 1;

    // If new size fits in current block, return same pointer
    if (block->size >= size) {
        return ptr;  // No need to move or copy
    }

    // Need to allocate new block (current is too small)
    void *new_ptr = my_malloc(size);
    if (new_ptr == NULL) {
        return NULL;  // Allocation failed
    }

    // Copy data from old block to new block
    // Copy the smaller of: old size or new size
    size_t copy_size = block->size < size ? block->size : size;
    memcpy(new_ptr, ptr, copy_size);

    // Free the old block
    my_free(ptr);

    return new_ptr;
}