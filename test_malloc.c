#include "my_malloc.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#define TEST(name) void name()
#define RUN_TEST(name) do { \
    printf("Running %s...", #name); \
    name(); \
    printf(" PASSED\n"); \
} while(0)

TEST(test_malloc_returns_non_null) {
    void *ptr = my_malloc(100);
    assert(ptr != NULL);
    my_free(ptr);
}

TEST(test_malloc_can_write_and_read) {
    char *ptr = my_malloc(100);
    assert(ptr != NULL);

    strcpy(ptr, "Hello, malloc!");
    assert(strcmp(ptr, "Hello, malloc!") == 0);

    my_free(ptr);
}

TEST(test_multiple_mallocs_dont_overlap) {
    int *ptr1 = my_malloc(sizeof(int));
    int *ptr2 = my_malloc(sizeof(int));

    assert(ptr1 != NULL);
    assert(ptr2 != NULL);
    assert(ptr1 != ptr2);

    *ptr1 = 42;
    *ptr2 = 99;

    assert(*ptr1 == 42);
    assert(*ptr2 == 99);

    my_free(ptr1);
    my_free(ptr2);
}

TEST(test_malloc_zero_returns_null) {
    void *ptr = my_malloc(0);
    assert(ptr == NULL);
}

// ========== calloc tests ==========

TEST(test_calloc_returns_non_null) {
    void *ptr = my_calloc(10, sizeof(int));
    assert(ptr != NULL);
    my_free(ptr);
}

TEST(test_calloc_zeroes_memory) {
    int *arr = my_calloc(10, sizeof(int));
    assert(arr != NULL);

    // Verify all bytes are zero
    for (int i = 0; i < 10; i++) {
        assert(arr[i] == 0);
    }

    my_free(arr);
}

TEST(test_calloc_array_allocation) {
    // Allocate array of 100 chars
    char *buffer = my_calloc(100, sizeof(char));
    assert(buffer != NULL);

    // Verify zeroed
    for (int i = 0; i < 100; i++) {
        assert(buffer[i] == 0);
    }

    // Write to it
    strcpy(buffer, "Testing calloc");
    assert(strcmp(buffer, "Testing calloc") == 0);

    my_free(buffer);
}

TEST(test_calloc_zero_nmemb_returns_null) {
    void *ptr = my_calloc(0, sizeof(int));
    assert(ptr == NULL);
}

TEST(test_calloc_zero_size_returns_null) {
    void *ptr = my_calloc(10, 0);
    assert(ptr == NULL);
}

TEST(test_calloc_overflow_protection) {
    // Try to overflow: SIZE_MAX / sizeof(int) + 1
    // This should be detected and return NULL
    size_t huge_count = SIZE_MAX / sizeof(int) + 1;
    void *ptr = my_calloc(huge_count, sizeof(int));
    assert(ptr == NULL);  // Should fail safely
}

// ========== realloc tests ==========

TEST(test_realloc_null_ptr_behaves_like_malloc) {
    // realloc(NULL, size) should behave like malloc(size)
    void *ptr = my_realloc(NULL, 100);
    assert(ptr != NULL);

    // Should be able to write to it
    char *str = (char *)ptr;
    strcpy(str, "Hello");
    assert(strcmp(str, "Hello") == 0);

    my_free(ptr);
}

TEST(test_realloc_zero_size_frees_and_returns_null) {
    // realloc(ptr, 0) should behave like free(ptr) and return NULL
    void *ptr = my_malloc(100);
    assert(ptr != NULL);

    void *result = my_realloc(ptr, 0);
    assert(result == NULL);
    // ptr is now freed, don't use it
}

TEST(test_realloc_larger_size_preserves_data) {
    // Allocate small, realloc to larger, verify data preserved
    char *ptr = my_malloc(10);
    assert(ptr != NULL);
    strcpy(ptr, "Hello");

    // Realloc to larger size (will likely move to new location)
    char *new_ptr = my_realloc(ptr, 100);
    assert(new_ptr != NULL);

    // Original data should be preserved
    assert(strcmp(new_ptr, "Hello") == 0);

    // Should be able to write more
    strcpy(new_ptr, "Hello, this is a much longer string!");
    assert(strcmp(new_ptr, "Hello, this is a much longer string!") == 0);

    my_free(new_ptr);
}

TEST(test_realloc_smaller_size_preserves_data) {
    // Allocate large, realloc to smaller, verify data preserved
    char *ptr = my_malloc(100);
    assert(ptr != NULL);
    strcpy(ptr, "Hello, World!");

    // Realloc to smaller size (should fit in place)
    void *old_addr = ptr;  // Save address for comparison
    char *new_ptr = my_realloc(ptr, 20);
    assert(new_ptr != NULL);

    // Should return same pointer (fits in place)
    assert(new_ptr == old_addr);

    // Data should still be there
    assert(strcmp(new_ptr, "Hello, World!") == 0);

    my_free(new_ptr);
}

TEST(test_realloc_same_size_returns_same_pointer) {
    // When size fits in current block, should return same pointer
    int *ptr = my_malloc(sizeof(int) * 10);
    assert(ptr != NULL);
    ptr[0] = 42;
    ptr[9] = 99;

    // Save the original address
    void *old_addr = ptr;

    // Realloc to same size
    int *new_ptr = my_realloc(ptr, sizeof(int) * 10);
    assert(new_ptr != NULL);

    // Should be the SAME pointer (fits in place, no move needed)
    assert(new_ptr == old_addr);

    // Data should be preserved
    assert(new_ptr[0] == 42);
    assert(new_ptr[9] == 99);

    my_free(new_ptr);
}

TEST(test_realloc_preserves_partial_data_when_shrinking) {
    // When reallocating to smaller size, partial data is preserved
    int *ptr = my_malloc(sizeof(int) * 10);
    assert(ptr != NULL);

    for (int i = 0; i < 10; i++) {
        ptr[i] = i * 10;
    }

    // Realloc to smaller (5 ints instead of 10)
    int *new_ptr = my_realloc(ptr, sizeof(int) * 5);
    assert(new_ptr != NULL);

    // First 5 elements should be preserved
    for (int i = 0; i < 5; i++) {
        assert(new_ptr[i] == i * 10);
    }

    my_free(new_ptr);
}

// ========== block reuse tests ==========

TEST(test_freed_block_is_reused) {
    // This tests that freed blocks are reused instead of calling mmap again
    // Use a large unique size to avoid reusing blocks from previous tests

    int initial_count = get_mmap_count();

    // Allocate a large block (>4KB forces new mmap)
    void *ptr1 = my_malloc(5000);
    assert(ptr1 != NULL);

    // Should have called mmap once
    int after_first_alloc = get_mmap_count();
    assert(after_first_alloc == initial_count + 1);

    // Free the block
    my_free(ptr1);

    // Allocate same size again
    void *ptr2 = my_malloc(5000);
    assert(ptr2 != NULL);

    // Should NOT have called mmap again (reused freed block)
    int after_second_alloc = get_mmap_count();
    assert(after_second_alloc == after_first_alloc);

    // The pointers should be the same (same block reused)
    assert(ptr1 == ptr2);

    my_free(ptr2);
}

TEST(test_block_splitting_creates_remainder) {
    // When allocating small from large block, should split and leave remainder

    int initial_count = get_mmap_count();

    // Allocate large block (forces new mmap of >4KB)
    void *ptr1 = my_malloc(6000);
    assert(ptr1 != NULL);
    int after_first = get_mmap_count();
    assert(after_first == initial_count + 1);

    // Free it (now we have a large ~6KB+ freed block)
    my_free(ptr1);

    // Allocate small block (should split the large freed block)
    void *ptr2 = my_malloc(50);
    assert(ptr2 != NULL);

    // Should reuse the freed block (no new mmap)
    int after_second = get_mmap_count();
    assert(after_second == after_first);

    // Allocate another small block (should use the split remainder)
    void *ptr3 = my_malloc(50);
    assert(ptr3 != NULL);

    // Should STILL not need mmap (uses split remainder)
    int after_third = get_mmap_count();
    assert(after_third == after_first);

    // ptr2 and ptr3 should be different (different parts of split block)
    assert(ptr2 != ptr3);

    my_free(ptr2);
    my_free(ptr3);
}

TEST(test_multiple_allocations_from_split_block) {
    // Test that multiple small allocations can use a large freed block

    int initial_count = get_mmap_count();

    // Allocate and free a large unique block
    void *large = my_malloc(7000);
    assert(large != NULL);

    int after_large_alloc = get_mmap_count();
    assert(after_large_alloc == initial_count + 1);

    my_free(large);

    // Allocate many small blocks - all should fit in the freed 7KB block with splitting
    void *ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = my_malloc(200);
        assert(ptrs[i] != NULL);
    }

    // Should not have needed additional mmap calls (all fit via splitting)
    int count_after_small = get_mmap_count();
    assert(count_after_small == after_large_alloc);

    // Clean up
    for (int i = 0; i < 10; i++) {
        my_free(ptrs[i]);
    }
}

TEST(test_coalescing_adjacent_freed_blocks) {
    // When adjacent blocks are freed, they should merge
    // Start fresh with a new large block to avoid state from previous tests

    int initial_count = get_mmap_count();

    // First create a large block that we'll split into three pieces
    void *temp = my_malloc(8000);
    assert(temp != NULL);
    int after_temp = get_mmap_count();
    assert(after_temp == initial_count + 1);
    my_free(temp);

    // Now allocate three blocks from this freed space (with splitting)
    void *ptr1 = my_malloc(500);
    void *ptr2 = my_malloc(500);
    void *ptr3 = my_malloc(500);
    assert(ptr1 != NULL && ptr2 != NULL && ptr3 != NULL);

    int after_allocs = get_mmap_count();
    // Should still be same count (all came from split)
    assert(after_allocs == after_temp);

    // Free them all (they should coalesce back together)
    my_free(ptr1);
    my_free(ptr2);
    my_free(ptr3);

    // Now allocate a block that needs coalesced space
    // (3 blocks of ~500+24 bytes each = ~1572 bytes available after coalescing)
    void *large = my_malloc(1500);
    assert(large != NULL);

    // Should not need new mmap if coalescing worked
    int after_large = get_mmap_count();
    assert(after_large == after_allocs);

    my_free(large);
}

TEST(test_coalescing_with_interleaved_frees) {
    // Test coalescing when blocks are freed in non-sequential order

    int initial_count = get_mmap_count();

    // Create fresh space to work with
    void *temp = my_malloc(9000);
    assert(temp != NULL);
    int after_temp = get_mmap_count();
    assert(after_temp == initial_count + 1);
    my_free(temp);

    // Allocate 5 blocks from this space
    void *ptr1 = my_malloc(400);
    void *ptr2 = my_malloc(400);
    void *ptr3 = my_malloc(400);
    void *ptr4 = my_malloc(400);
    void *ptr5 = my_malloc(400);

    assert(ptr1 && ptr2 && ptr3 && ptr4 && ptr5);

    int after_allocs = get_mmap_count();
    // Should all come from split (no new mmap)
    assert(after_allocs == after_temp);

    // Free blocks 1, 3, 5 (non-adjacent in free list)
    my_free(ptr1);
    my_free(ptr3);
    my_free(ptr5);

    // Now free blocks 2 and 4 (should coalesce with neighbors)
    my_free(ptr2);  // Should coalesce with ptr1
    my_free(ptr4);  // Should coalesce with ptr3 and ptr5

    // Try to allocate something that needs coalesced space
    // (5 blocks of ~400+24 bytes = ~2120 bytes available after coalescing)
    void *large = my_malloc(2000);
    assert(large != NULL);

    // Should reuse coalesced blocks (no new mmap)
    int after_large = get_mmap_count();
    assert(after_large == after_allocs);

    my_free(large);
}

TEST(test_allocation_too_large_needs_new_mmap) {
    // If no free block is large enough, should call mmap
    // Use a very large size to ensure it doesn't fit in any existing free blocks

    int initial = get_mmap_count();

    // Allocate something HUGE (100KB) - definitely needs new mmap
    void *huge1 = my_malloc(100000);
    assert(huge1 != NULL);

    int after_first_huge = get_mmap_count();
    assert(after_first_huge == initial + 1);

    my_free(huge1);

    // Now allocate even larger (200KB) - won't fit in freed 100KB block
    void *huge2 = my_malloc(200000);
    assert(huge2 != NULL);

    // Should have called mmap again
    int after_second_huge = get_mmap_count();
    assert(after_second_huge == after_first_huge + 1);

    my_free(huge2);
}

int main() {
    printf("=== My Malloc TDD Test Suite ===\n\n");

    printf("--- malloc tests ---\n");
    RUN_TEST(test_malloc_returns_non_null);
    RUN_TEST(test_malloc_can_write_and_read);
    RUN_TEST(test_multiple_mallocs_dont_overlap);
    RUN_TEST(test_malloc_zero_returns_null);

    printf("\n--- calloc tests ---\n");
    RUN_TEST(test_calloc_returns_non_null);
    RUN_TEST(test_calloc_zeroes_memory);
    RUN_TEST(test_calloc_array_allocation);
    RUN_TEST(test_calloc_zero_nmemb_returns_null);
    RUN_TEST(test_calloc_zero_size_returns_null);
    RUN_TEST(test_calloc_overflow_protection);

    printf("\n--- realloc tests ---\n");
    RUN_TEST(test_realloc_null_ptr_behaves_like_malloc);
    RUN_TEST(test_realloc_zero_size_frees_and_returns_null);
    RUN_TEST(test_realloc_larger_size_preserves_data);
    RUN_TEST(test_realloc_smaller_size_preserves_data);
    RUN_TEST(test_realloc_same_size_returns_same_pointer);
    RUN_TEST(test_realloc_preserves_partial_data_when_shrinking);

    printf("\n--- block reuse tests ---\n");
    RUN_TEST(test_freed_block_is_reused);
    RUN_TEST(test_block_splitting_creates_remainder);
    RUN_TEST(test_multiple_allocations_from_split_block);
    RUN_TEST(test_coalescing_adjacent_freed_blocks);
    RUN_TEST(test_coalescing_with_interleaved_frees);
    RUN_TEST(test_allocation_too_large_needs_new_mmap);

    printf("\nAll tests passed!\n");
    return 0;
}
