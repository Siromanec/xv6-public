//
// Created by ADMIN on 29-Oct-23.
//
#include "types.h"
#include "user.h"


int main(void) {
  uint size = 4096 * 4;
  char * my_mem = malloc(size);

  for (int i = 0; i < size; ++i) {
    if (!(i%8))
      sleep(1);
    *(my_mem + i) = 'c';
  }
  printf(STDOUT, "FIN\n");
  free(my_mem);
  exit();
}