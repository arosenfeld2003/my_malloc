# My Malloc: Design Document

Last updated: 1/6/26

## 1. Architecture Overview

This memory allocator uses **in-band metadata** with a **doubly-linked list** to track all memory blocks (both free and allocated).

### High-Level Design Diagram

```
User calls my_malloc(100)
         |
         v
   Search linked list for free block >= 100 bytes
         |
         +-- Found? --> Split if large enough
         |              Mark allocated
         |              Return pointer
         |
         +-- Not found? --> mmap(4096+ bytes)
                            |
                            v
                      Create header, add to list, return pointer

User calls my_free(ptr)
         |
         v
   Find header (ptr - 1)
         |
         v
   Mark block as free
         |
         v
   Coalesce with adjacent free blocks (if any)
```

---

## 2. Metadata Strategy: In-Band Headers

### Decision: Store metadata INSIDE each allocated block

Each memory block has a **32-byte header** immediately before the user data:

```
Memory Layout:
┌──────────────────────┐
│ size (8 bytes)       │  User data size (excludes header)
├──────────────────────┤
│ is_free (1 byte)     │  0 = allocated, 1 = free
├──────────────────────┤
│ padding (7 bytes)    │  For 8-byte alignment
├──────────────────────┤
│ next (8 bytes)       │  Pointer to next block in list
├──────────────────────┤
│ prev (8 bytes)       │  Pointer to previous block in list
├──────────────────────┤ <- my_malloc returns pointer HERE (header + 1)
│                      │
│ User Data            │
│ (size bytes)         │
│                      │
└──────────────────────┘
```

### Pointer Arithmetic

```c
// block points to address 0x1000 (start of header)
block_header_t *block = (block_header_t *)0x1000;

// block + 1 means: 0x1000 + sizeof(block_header_t) = 0x1000 + 32 = 0x1020
void *user_ptr = (void *)(block + 1);  // Returns 0x1020

// To get header from user pointer (reverse):
block_header_t *header = (block_header_t *)user_ptr - 1;  // Goes from 0x1020 -> 0x1000
```

### Trade-offs

**Pros:**
- Simple pointer arithmetic (just `ptr + 1` and `ptr - 1`)
- No separate tracking structure needed
- Cache-friendly (metadata near data)
- Doubly-linked list enables O(1) coalescing

**Cons:**
- 32 bytes overhead per allocation (increased from 24 to support prev pointer)
- Potential fragmentation (mitigated by coalescing)
- Metadata can be corrupted by buffer overflows

---

## 3. Search Structure: Doubly-Linked List

### Decision: Doubly-linked list with head pointer

```
head (global variable)
  |
  v
NULL <- ┌─────┐ <-> ┌─────┐ <-> ┌─────┐ -> NULL
        │Block│     │Block│     │Block│
        │ 1   │     │ 2   │     │ 3   │
        └─────┘     └─────┘     └─────┘
         FREE      ALLOCATED     FREE
```

### Implementation

- Global `head` pointer points to most recently allocated block
- New blocks are inserted at the head (O(1) insertion)
- Search is **first-fit**: traverse list until finding first free block that fits (O(n) search)
- Each block has both `next` and `prev` pointers for bidirectional traversal

### Trade-offs

**Pros:**
- Simple to implement and debug
- Teaches fundamental concepts
- O(1) coalescing with prev pointer (no backward list traversal needed)
- No complex data structure overhead

**Cons:**
- O(n) search time (slow for many allocations)
- Extra 8 bytes per block for `prev` pointer
- No spatial locality optimization
- Could be improved with hash table or BST later

**Why doubly-linked?**
The `prev` pointer enables efficient O(1) coalescing by allowing immediate access to the previous block without traversing the entire list.

---

## 4. Memory Allocation: mmap Strategy

### System Call

```c
void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
```

- `MAP_ANONYMOUS`: Not backed by file (pure memory)
- `MAP_PRIVATE`: Private to this process
- Minimum allocation: **4096 bytes** (standard page size)

### When mmap is Called

1. Linked list is empty (first allocation)
2. No free block in list fits the requested size

### Current Behavior

```c
size_t alloc_size = HEADER_SIZE + size;  // 32 + user_size
if (alloc_size < MMAP_BLOCK_SIZE) {
    alloc_size = MMAP_BLOCK_SIZE;  // Round up to 4096
}
```

**Example**: User requests 100 bytes
- Total needed: 32 (header) + 100 (data) = 132 bytes
- Actually allocated: 4096 bytes
- Remainder: 3964 bytes (split into separate free block if >= 32 + 32 bytes)

---

## 5. Split/Coalesce Strategy

### Current State: ✅ IMPLEMENTED

#### Block Splitting (IMPLEMENTED)

**Problem**: Allocating 100 bytes from a 4KB block wastes 3900+ bytes

**Solution**: Split large blocks when allocating
```
Before: [FREE 4096 bytes]
After:  [ALLOCATED 100 bytes] -> [FREE 3932 bytes]
                                   (new block in list)
```

**Implementation**:
- When a free block is found that's >= `size + HEADER_SIZE + 32`, split it
- Minimum split remainder: 32 bytes user data (avoids tiny unusable blocks)
- Creates new free block in linked list for the remainder
- Time complexity: O(1) split operation

**Example**:
```c
// User requests 100 bytes, free block has 4000 bytes
// Split threshold: 100 + 32 (header) + 32 (min remainder) = 164 bytes
// 4000 >= 164, so split occurs:
// Block 1: size=100, is_free=0 (allocated to user)
// Block 2: size=3868, is_free=1 (remainder, available for reuse)
```

#### Coalescing (IMPLEMENTED)

**Problem**: Many small free blocks scattered in memory (fragmentation)

**Solution**: Immediate coalescing on `my_free()`
```
Before free:  [FREE] <-> [ALLOCATED X] <-> [FREE]
After free:   [FREE..............................]  (merged into one large block)
```

**Implementation**:
- **Strategy**: Immediate coalescing (merge on every free)
- Checks both next and previous blocks using doubly-linked list
- Only merges if blocks are physically adjacent in memory
- Updates linked list pointers to remove absorbed blocks
- Time complexity: O(1) using prev pointer

**Adjacency Check**:
```c
// For next block
void *expected_next_addr = (char *)(block + 1) + block->size;
if ((void *)next == expected_next_addr && next->is_free) {
    // Merge next into block
}

// For previous block
void *expected_block_addr = (char *)(prev + 1) + prev->size;
if ((void *)block == expected_block_addr && prev->is_free) {
    // Merge block into prev
}
```

**Why immediate coalescing?**
- Reduces fragmentation immediately
- Simpler to reason about (consistent state after every free)
- O(1) performance with doubly-linked list (no traversal needed)

---

## 6. Function Specifications

### my_malloc(size_t size)

**Algorithm**:
1. If size == 0, return NULL
2. Search linked list for free block where `block->size >= size`
3. If found and block is large enough to split:
   - Split block (minimum 32 bytes remainder)
   - Create new free block for remainder
   - Update doubly-linked list pointers
   - Mark current block `is_free = 0`
   - Return `block + 1`
4. If found but not large enough to split:
   - Mark `is_free = 0`
   - Return `block + 1`
5. If not found:
   - Call `mmap()` for new block (minimum 4096 bytes)
   - Initialize header with next/prev pointers
   - Insert at head of list
   - Return `block + 1`

**Time complexity**: O(n) where n = number of blocks

### my_free(void *ptr)

**Algorithm**:
1. If ptr == NULL, return
2. Get header: `block = (block_header_t *)ptr - 1`
3. Mark `is_free = 1`
4. Coalesce with next block (if free and adjacent):
   - Merge next into current block
   - Update linked list pointers
5. Coalesce with previous block (if free and adjacent):
   - Merge current into previous block
   - Update linked list pointers

**Time complexity**: O(1) (doubly-linked list enables immediate prev access)

### my_calloc(size_t nmemb, size_t size)

**Algorithm**:
1. Check for overflow: `nmemb > SIZE_MAX / size`
2. If overflow, return NULL
3. Call `my_malloc(nmemb * size)`
4. `memset()` to zero
5. Return pointer

**Time complexity**: O(n + m) where n = malloc time, m = bytes to zero

### my_realloc(void *ptr, size_t size)

**Status**: ✅ IMPLEMENTED

**Algorithm**:
1. If ptr == NULL, behave like `my_malloc(size)`
2. If size == 0, behave like `my_free(ptr)` and return NULL
3. Get current block header
4. If `current_size >= size`, return ptr (fits in place)
5. Otherwise:
   - Call `my_malloc(size)`
   - Copy `min(current_size, size)` bytes using `memcpy()`
   - Call `my_free(ptr)`
   - Return new pointer

**Time complexity**: O(n) for search + O(m) for copy where m = bytes to copy

---

## 7. Testing Strategy

### Current Test Coverage (22/22 passing ✅)

**malloc tests** (4 tests):
- Non-null return ✓
- Write/read data ✓
- Multiple allocations don't overlap ✓
- Zero-size returns NULL ✓

**calloc tests** (6 tests):
- Non-null return ✓
- Memory zeroed ✓
- Array allocation ✓
- Zero nmemb/size returns NULL ✓
- Overflow protection ✓
- Large array allocation ✓

**realloc tests** (6 tests):
- NULL ptr behaves like malloc ✓
- Size 0 frees and returns NULL ✓
- Resize larger preserves data ✓
- Resize smaller preserves data ✓
- Same size returns same pointer ✓
- Partial data preserved when shrinking ✓

**block reuse tests** (6 tests):
- Freed block is reused ✓
- Block splitting creates remainder ✓
- Multiple allocations from split block ✓
- Coalescing adjacent freed blocks ✓
- Coalescing with interleaved frees ✓
- Large allocation needs new mmap ✓

### Testing Tools

- **`get_mmap_count()`**: Debug function to verify block reuse
- Monitors number of mmap calls to ensure splitting/coalescing works
- Tests verify no unnecessary mmap calls occur when blocks can be reused

---

## 8. Known Limitations

1. **No munmap**: Memory never returned to OS (grows indefinitely)
2. **First-fit search**: Slow O(n) for many allocations
3. **No alignment enforcement**: Relies on mmap returning aligned memory
4. **No thread safety**: Global `head` pointer not protected
5. **32-byte overhead**: Each allocation has 32-byte header (doubly-linked list cost)
6. **Minimum block size**: Split blocks must have >= 32 bytes user data (prevents tiny blocks)

---

## 9. Completed Features ✅

1. ✅ **my_malloc()** - Allocation with block splitting
2. ✅ **my_free()** - Deallocation with immediate coalescing
3. ✅ **my_calloc()** - Zero-initialized allocation with overflow protection
4. ✅ **my_realloc()** - Resize allocation with data preservation
5. ✅ **Block splitting** - Reduces memory waste from large free blocks
6. ✅ **Coalescing** - Merges adjacent free blocks to prevent fragmentation
7. ✅ **Doubly-linked list** - O(1) coalescing performance
8. ✅ **Comprehensive testing** - 22 tests covering all functionality

## 10. Future Optimizations (Optional)

1. **Best-fit search** - Find smallest sufficient block instead of first-fit
2. **Segregated free lists** - Separate lists by size class for faster search
3. **munmap support** - Return large free blocks to OS
4. **Thread safety** - Add mutex protection for concurrent access
5. **Boundary tags** - Store size at end of block for faster coalescing
6. **Alignment control** - Support custom alignment requirements

---

## 11. References

- [mmap(2) man page](https://man7.org/linux/man-pages/man2/mmap.2.html)
- [malloc(3) man page](https://man7.org/linux/man-pages/man3/malloc.3.html)
- Buffer overflow prevention: [Stack Overflow discussion](https://stackoverflow.com/questions/9901366/try-to-buffer-overflow-value-allocated-by-malloc)
