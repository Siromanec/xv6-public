//
// Created by ADMIN on 24-Oct-23.
//

#ifndef XV6_PUBLIC_SWAP_H
#define XV6_PUBLIC_SWAP_H
#define SWBLOCKS (PGSIZE / BSIZE)

struct swap_s {
  uint block; // first block of four that stores the page
  void *va; // virtual address it represents
  uint pa;
  BOOL taken;
//  struct proc *proc;
  struct sleeplock lock;
  struct swap_s *prev;
  struct swap_s *next;
};

#define SWAP_RANDOM
#define SWAP_LRU
#define SWAP_FIFO



#endif //XV6_PUBLIC_SWAP_H
