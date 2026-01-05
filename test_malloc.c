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

    printf("\nAll tests passed!\n");
    return 0;
}
