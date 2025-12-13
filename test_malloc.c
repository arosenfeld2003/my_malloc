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

    printf("\nAll tests passed!\n");
    return 0;
}
