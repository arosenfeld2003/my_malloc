#include "my_malloc.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

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

int main() {
    printf("=== My Malloc TDD Test Suite ===\n\n");

    RUN_TEST(test_malloc_returns_non_null);
    RUN_TEST(test_malloc_can_write_and_read);
    RUN_TEST(test_multiple_mallocs_dont_overlap);
    RUN_TEST(test_malloc_zero_returns_null);

    printf("\nAll tests passed!\n");
    return 0;
}
