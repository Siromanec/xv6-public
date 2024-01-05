//
// Created by ADMIN on 29-Oct-23.
//
#include "types.h"
#include "user.h"

#define new(type, size) malloc(size * sizeof (type))

int main(int argc, char ** argv) {
  uint size = 4096;
//  int * my_mem = malloc(size * sizeof (int));

  int * my_mem = new(int, size);
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