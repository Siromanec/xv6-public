//
// Created by ADMIN on 24-Oct-23.
//

#ifndef XV6_PUBLIC_SWAP_H
#define XV6_PUBLIC_SWAP_H
#define SWBLOCKS (PGSIZE / BSIZE)

struct swap_s {
  uint block; // first block of four that stores the page
  uint va; // virtual address it represents
  int taken;
  struct proc *proc;
  pde_t *pte;
  struct sleeplock lock;
  struct swap_s *prev;
  struct swap_s *next;
};






#endif //XV6_PUBLIC_SWAP_H
