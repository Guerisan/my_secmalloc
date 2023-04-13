#include <stdio.h>
#include "my_secmalloc.h"

//void *malloc(size_t size){
// void *(*pfm)(size_t) = dlsym(RTLD_NEXT, "malloc");
// printf("old malloc %p\n", pfm);
// return pfm(size);
//}

int main(int ac, char**av){
  (void) ac;
  (void) av;
  char *truc = malloc(55);
  printf("truc %s\n", truc);
  return 0;
} 
