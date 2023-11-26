//
// Created by ADMIN on 29-Oct-23.
//
#include "types.h"
#include "user.h"


int main(void) {
  uint size = 4096 * 4;
  int * my_mem = malloc(size);

  for (int i = 0; i < size; ++i) {
    if (!(i%8))
      sleep(1);
    *(my_mem + i) = i;
  }
  uint cumsum = 0;
  for (int i = size - 1; i >= 0; --i) {
    cumsum += my_mem[i];
  }
  printf(STDOUT, "FIN CUMSUM = %u\n", cumsum);
  free(my_mem);
  exit();
}