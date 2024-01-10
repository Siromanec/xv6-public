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

#define SWAPFILE
void swapinit_file(void);
void swapwrite_file(const char *buf, void * la, pte_t * buf_pte);


#endif //XV6_PUBLIC_SWAP_H
