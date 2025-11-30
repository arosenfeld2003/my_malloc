#ifndef MY_MALLOC_H
#define MY_MALLOC_H

#include <stdlib.h>

void *my_malloc(size_t size);
void my_free(void *ptr);
void *mycalloc(size_t nmemb, size_t size);
void *myrealloc(void *ptr, size_t size);

#endif