// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"
#include "debug.h"

void freerange(void *vstart, void *vend);

extern char end[]; // first address after kernel loaded from ELF file
// defined by the kernel linker script in kernel.ld
struct {
  struct spinlock lock;
  int data[PHYSTOP / PGSIZE];
} phys_page_refcount;
struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend) {
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend) {
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend) {
  char *p;
  p = (char *) PGROUNDUP((uint) vstart);
  for (; p + PGSIZE <= (char *) vend; p += PGSIZE)
    kfree(p);
}

//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v) {
  struct run *r;

  if ((uint) v % PGSIZE || v < end || V2P(v) >= PHYSTOP) {
    cprintf("0x%x", v);
    cprintf("%d\n", (uint) v % PGSIZE);
    cprintf("%d\n", v < end); // this check was designed for when we couldnt swap elf pages of the kernel
    cprintf("%d\n", V2P(v) >= PHYSTOP);
    panic("kfree");
  }



  // Fill with junk to catch dangling refs.

  r = (struct run *) v;

  if (phys_page_refcount.data[V2P(r) / PGSIZE] == 1) // last reference // TODO determine wether locking is requiredd here
    memset(v, 1, PGSIZE); // it needs to be here because it makes rnext 0xfffffffff
//  cprintf("0x%x\n", V2P(r) / PGSIZE);

  if (kmem.use_lock) {
    acquire(&kmem.lock);
    acquire(&phys_page_refcount.lock);
  }
  if (phys_page_refcount.data[V2P(r) / PGSIZE] > 0) { // double kfree used to work fine
//    if(kmem.use_lock){
//      cprintf("fake kfree\n");
//    }
    phys_page_refcount.data[V2P(r) / PGSIZE]--;
  }

  if (phys_page_refcount.data[V2P(r) / PGSIZE] == 0) {
//    if(kmem.use_lock){
//      cprintf("kfree\n");
//    }
    r->next = kmem.freelist;
    kmem.freelist = r;
  }


  if (kmem.use_lock) {
    release(&phys_page_refcount.lock);
    release(&kmem.lock);
  }


}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char *
kalloc(void) {
  struct run *r;

  if (kmem.use_lock) {
    acquire(&kmem.lock);
    acquire(&phys_page_refcount.lock);
  }
  r = kmem.freelist;
  if (r != NULL) {
    kmem.freelist = r->next;
    phys_page_refcount.data[V2P(r) / PGSIZE] = 1;
  }
  if (kmem.use_lock) {
    release(&phys_page_refcount.lock);
    release(&kmem.lock);
  }
  return (char *) r;
}

int inc_ref_pa(uint pa) {
//  cprintf("0x%x\n",pa/PGSIZE);

  if (pa > PHYSTOP)
    panic("inc above PHYSTOP");
#ifdef DEBUG_PGREFCNT
  cprintf("inc: 0x%x from:%d to:%d\n", pa,phys_page_refcount.data[pa/PGSIZE],phys_page_refcount.data[pa/PGSIZE] + 1 );
#endif

  acquire(&phys_page_refcount.lock);
  int c = ++phys_page_refcount.data[pa/PGSIZE];
  release(&phys_page_refcount.lock);
  return c;

}
int get_ref_pa(uint pa){

  if (pa > PHYSTOP)
    panic("get above PHYSTOP");
  acquire(&phys_page_refcount.lock);
  int c = phys_page_refcount.data[pa/PGSIZE];
  release(&phys_page_refcount.lock);

  return c;
}

int dec_ref_pa(uint pa) {
  if (pa > PHYSTOP)
    panic("dec above PHYSTOP");
#ifdef DEBUG_PGREFCNT
  cprintf("dec: 0x%x from:%d to:%d\n", pa,phys_page_refcount.data[pa/PGSIZE],phys_page_refcount.data[pa/PGSIZE] - 1  );
#endif
  acquire(&phys_page_refcount.lock);
  if (phys_page_refcount.data[pa / PGSIZE] > 0) {
    int c = --phys_page_refcount.data[pa / PGSIZE];
    release(&phys_page_refcount.lock);
    return c;
  }
  release(&phys_page_refcount.lock);
 panic("dec_ref_pa: decrementing no ref");
// return 0 ;
}
