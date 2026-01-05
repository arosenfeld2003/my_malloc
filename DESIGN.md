# My Malloc: Design Document

## 1. Architecture Overview

This memory allocator uses **in-band metadata** with a **single linked list** to track all memory blocks (both free and allocated).

### High-Level Design Diagram

```
User calls my_malloc(100)
         |
         v
   Search linked list for free block >= 100 bytes
         |
         +-- Found? --> Mark allocated, return pointer
         |
         +-- Not found? --> mmap(4096 bytes)
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
   [TODO: Coalesce with neighbors]
```

---

## 2. Metadata Strategy: In-Band Headers

### Decision: Store metadata INSIDE each allocated block

Each memory block has a **24-byte header** immediately before the user data:

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

// block + 1 means: 0x1000 + sizeof(block_header_t) = 0x1000 + 24 = 0x1018
void *user_ptr = (void *)(block + 1);  // Returns 0x1018

// To get header from user pointer (reverse):
block_header_t *header = (block_header_t *)user_ptr - 1;  // Goes from 0x1018 -> 0x1000
```

### Trade-offs

**Pros:**
- Simple pointer arithmetic (just `ptr + 1` and `ptr - 1`)
- No separate tracking structure needed
- Cache-friendly (metadata near data)

**Cons:**
- 24 bytes overhead per allocation
- Potential fragmentation
- Metadata can be corrupted by buffer overflows

---

## 3. Search Structure: Linked List

### Decision: Single linked list with head pointer

```
head (global variable)
  |
  v
┌─────┐    ┌─────┐    ┌─────┐
│Block│───>│Block│───>│Block│───> NULL
│ 1   │    │ 2   │    │ 3   │
└─────┘    └─────┘    └─────┘
 FREE      ALLOCATED   FREE
```

### Implementation

- Global `head` pointer points to most recently allocated block
- New blocks are inserted at the head (O(1) insertion)
- Search is **first-fit**: traverse list until finding first free block that fits (O(n) search)

### Trade-offs

**Pros:**
- Simple to implement and debug
- Teaches fundamental concepts
- No complex data structure overhead

**Cons:**
- O(n) search time (slow for many allocations)
- No spatial locality optimization
- Could be improved with hash table or BST later

**Why linked list first?**
Per dev_plan.txt: "linked lists let you understand the core problem before optimization"

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
size_t alloc_size = HEADER_SIZE + size;  // 24 + user_size
if (alloc_size < MMAP_BLOCK_SIZE) {
    alloc_size = MMAP_BLOCK_SIZE;  // Round up to 4096
}
```

**Example**: User requests 100 bytes
- Total needed: 24 (header) + 100 (data) = 124 bytes
- Actually allocated: 4096 bytes
- Waste: 3972 bytes (but marked as allocated, so NOT reusable)

---

## 5. Split/Coalesce Strategy

### Current State: NO Splitting, NO Coalescing

#### Splitting (TODO)

**Problem**: Allocating 100 bytes from a 4KB block wastes 3900+ bytes

**Current behavior**:
- Entire 4KB block is marked as allocated
- Remaining space cannot be reused

**Future solution**: Split large blocks
```
Before: [FREE 4096 bytes]
After:  [ALLOCATED 124 bytes] -> [FREE 3972 bytes]
                                   (new block in list)
```

#### Coalescing (TODO)

**Problem**: Many small free blocks scattered in memory (fragmentation)

**Current behavior**:
- `my_free()` marks block as free
- Does NOT merge with adjacent free blocks

**Future solution**: Immediate coalescing on free()
```
Before free:  [FREE] -> [ALLOCATED X] -> [FREE]
After free:   [FREE........................]  (merged into one large block)
```

**Design decision needed**:
- **Immediate coalescing**: Merge on every `my_free()` call
  - Pro: Less fragmentation
  - Con: Slower free() operations

- **Lazy coalescing**: Merge only when `my_malloc()` can't find space
  - Pro: Faster free()
  - Con: More fragmentation until allocation pressure

**Current decision**: Deferred until Phase 3

---

## 6. Function Specifications

### my_malloc(size_t size)

**Algorithm**:
1. If size == 0, return NULL
2. Search linked list for free block where `block->size >= size`
3. If found:
   - Mark `is_free = 0`
   - Return `block + 1`
4. If not found:
   - Call `mmap()` for new block (minimum 4096 bytes)
   - Initialize header
   - Insert at head of list
   - Return `block + 1`

**Time complexity**: O(n) where n = number of blocks

### my_free(void *ptr)

**Algorithm**:
1. If ptr == NULL, return
2. Get header: `block = (block_header_t *)ptr - 1`
3. Mark `is_free = 1`
4. [TODO: Coalesce with neighbors]

**Time complexity**: O(1) (no coalescing yet)

### my_calloc(size_t nmemb, size_t size)

**Algorithm**:
1. Check for overflow: `nmemb > SIZE_MAX / size`
2. If overflow, return NULL
3. Call `my_malloc(nmemb * size)`
4. `memset()` to zero
5. Return pointer

**Time complexity**: O(n + m) where n = malloc time, m = bytes to zero

### my_realloc(void *ptr, size_t size)

**Status**: Not implemented yet (stub returns NULL)

**Planned algorithm** (Phase 3):
1. If ptr == NULL, behave like `my_malloc(size)`
2. If size == 0, behave like `my_free(ptr)` and return NULL
3. Get current block header
4. If `current_size >= size`, return ptr (fits in place)
5. Otherwise:
   - Call `my_malloc(size)`
   - Copy `min(current_size, size)` bytes
   - Call `my_free(ptr)`
   - Return new pointer

---

## 7. Testing Strategy

### Current Test Coverage (10/10 passing)

**malloc tests**:
- Non-null return ✓
- Write/read data ✓
- Multiple allocations don't overlap ✓
- Zero-size returns NULL ✓

**calloc tests**:
- Non-null return ✓
- Memory zeroed ✓
- Array allocation ✓
- Zero nmemb/size returns NULL ✓
- Overflow protection ✓

### Future Tests Needed

- **realloc**: Resize smaller, resize larger, NULL edge cases
- **Coalescing**: Verify adjacent free blocks merge
- **Fragmentation**: Stress test with random alloc/free patterns
- **Memory leaks**: Verify all `mmap()` can be freed (munmap later?)

---

## 8. Known Limitations

1. **No splitting**: Wastes memory (allocating 100 bytes uses 4KB)
2. **No coalescing**: Fragmentation accumulates over time
3. **No munmap**: Memory never returned to OS (grows indefinitely)
4. **First-fit search**: Slow O(n) for many allocations
5. **No alignment enforcement**: Relies on mmap returning aligned memory
6. **No thread safety**: Global `head` pointer not protected
7. **No realloc**: Not implemented yet

---

## 9. Next Steps (Phase 3 from dev_plan.txt)

1. **Implement my_realloc()**
   - Handle NULL ptr edge case
   - Handle size == 0 edge case
   - Implement copy-and-free strategy

2. **Add coalescing**
   - Decide: immediate vs lazy
   - Merge adjacent free blocks
   - Update linked list structure

3. **Optimize (if time)**
   - First-fit vs best-fit comparison
   - Block splitting for large allocations
   - Consider free list separate from allocated list

---

## 10. References

- [mmap(2) man page](https://man7.org/linux/man-pages/man2/mmap.2.html)
- [malloc(3) man page](https://man7.org/linux/man-pages/man3/malloc.3.html)
- Buffer overflow prevention: [Stack Overflow discussion](https://stackoverflow.com/questions/9901366/try-to-buffer-overflow-value-allocated-by-malloc)
