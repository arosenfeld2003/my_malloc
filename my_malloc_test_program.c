#include "my_malloc.h"
  #include <stdio.h>

  int main() {
      void *ptr1 = my_malloc(100);
      void *ptr2 = my_malloc(200);

      printf("Allocated ptr1: %p\n", ptr1);
      printf("Allocated ptr2: %p\n", ptr2);
      printf("Total mmap calls: %d\n", get_mmap_count());

      my_free(ptr1);
      my_free(ptr2);

      return 0;
  }
