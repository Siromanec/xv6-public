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
#include "proc.h"
void freerange(void *vstart, void *vend);

extern char end[]; // first address after kernel loaded from ELF file
// defined by the kernel linker script in kernel.ld

#define NPDATAMAP (PHYSTOP / PGSIZE)
struct {
  struct spinlock lock;
  int use_lock;
  page_data_t data[NPDATAMAP];
} phys_page_data;




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
  initlock(&phys_page_data.lock, "phys_page_data");
  kmem.use_lock = 0;
  phys_page_data.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend) {
  freerange(vstart, vend);
  phys_page_data.use_lock = 1;
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

  if (get_ref_pa(V2P(r)) == 1) // last reference // TODO determine whether locking is requiredd here
    memset(v, 1, PGSIZE); // it needs to be here because it makes rnext 0xfffffffff -- later note: it does not it sets to 0x1 not 0xff

  if (get_ref_pa(V2P(r)) != 0)
    dec_ref_pa(V2P(r));


  if (kmem.use_lock) {
    acquire(&kmem.lock);
  }

  // is empty
  if (get_ref_pa(V2P(r)) == 0) {
    r->next = kmem.freelist;
    kmem.freelist = r;
  }

  if (kmem.use_lock) {
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
  }
  r = kmem.freelist;
  if (r != NULL) {
    kmem.freelist = r->next;
    // here i am hoping that refcount was set to zero - it had to be by kfree
    if (inc_ref_pa(V2P(r)) != 1)
      panic("kalloc: inc_ref_pa");
  } else {
    // TODO try to get a page that was swapped out
    // swapvictim();
    // r = kmem.freelist;
  }
  if (kmem.use_lock) {
    release(&kmem.lock);
  }
  return (char *) r;
}

static inline int __inc_ref_pa(uint pa) {
  return ++phys_page_data.data[pa / PGSIZE].ref_count;
}
static inline int __get_ref_pa(uint pa) {
  return phys_page_data.data[pa / PGSIZE].ref_count;
};
static inline int __dec_ref_pa(uint pa) {
  return --phys_page_data.data[pa / PGSIZE].ref_count;
}

int inc_ref_pa(uint pa) {
//  cprintf("0x%x\n",pa/PGSIZE);

  if (pa > PHYSTOP)
    panic("inc_ref_pa: above PHYSTOP");
#ifdef DEBUG_PGREFCNT
  cprintf("inc: 0x%x from:%d to:%d\n", pa,phys_page_data.data[pa/PGSIZE],phys_page_data.data[pa/PGSIZE] + 1 );
#endif

  if (phys_page_data.use_lock) {
    acquire(&phys_page_data.lock);
  }
  int cnt = __inc_ref_pa(pa);

  if (phys_page_data.use_lock) {
    release(&phys_page_data.lock);
  }
  return cnt;

}


int get_ref_pa(uint pa) {

  if (pa > PHYSTOP)
    panic("get_ref_pa: above PHYSTOP");
  if (phys_page_data.use_lock) {
    acquire(&phys_page_data.lock);
  }
  int cnt = __get_ref_pa(pa);

  if (phys_page_data.use_lock) {
    release(&phys_page_data.lock);
  }

  return cnt;
}



int dec_ref_pa(uint pa) {
  if (pa > PHYSTOP)
    panic("dec_ref_pa: above PHYSTOP");
#ifdef DEBUG_PGREFCNT
  cprintf("dec: 0x%x from:%d to:%d\n", pa,phys_page_data.data[pa/PGSIZE],phys_page_data.data[pa/PGSIZE] - 1  );
#endif
  if (phys_page_data.use_lock) {
    acquire(&phys_page_data.lock);
  }

  int cnt = __dec_ref_pa(pa);

  if (phys_page_data.use_lock) {
    release(&phys_page_data.lock);
  }
  if (cnt < 0)
    panic("dec_ref_pa: decrementing no ref");
// return 0 ;
  return cnt;
}

page_info_type_t __pa_get_type(uint pa) {
  if (__get_ref_pa(pa) == 1)
    return POINTS_TO_PTE;
  if(__get_ref_pa(pa) == 0)
    return -1;
  return VA;
}

pte_t ** __pa_get_pte_iterator(uint pa) {
  //TODO NOT READY
  if (__get_ref_pa(pa) == 0)
    return NULL;
  if (__get_ref_pa(pa) == 1)
  {
    return &phys_page_data.data[pa / PGSIZE].pte;
  }
  return NULL;
  // x_iterator_init (&x_iterator);
  // while(x_has_next(&x_iterator)):
  //   x_get_next(&x_iterator)
  //
  // for proc in ptable
  //   pgdir = proc.pgdir
  //   pte = walkpgdir(p->pgdir, phys_page_data.data[pa / PGSIZE].va, FALSE)
  //   if pte
  //   perform_action_for_single_pte(pte)
  //    PTE_FLAGS(*phys_page_data.data[pa / PGSIZE].pte);
}



//    pa_pte_iterator_get_next
// each concrete iterator must have a magic number (for type safety) (at the end?)
// or type + magic number
#include "types.h"
#include "stat.h"
#include "param.h"
#include "mmu.h"

// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.

typedef long Align;

union header {
  struct {
    union header *ptr;
    uint size;
  } s;
  Align x;
};

typedef union header Header;

static Header base;
static Header *freep = NULL;

void
kmallocfree(void *ap) {
  Header *bp, *p;

  bp = (Header *) ap - 1;
  for (p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    if (p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;
  if (bp + bp->s.size == p->s.ptr) {
    bp->s.size += p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  } else
    bp->s.ptr = p->s.ptr;
  if (p + p->s.size == bp) {
    p->s.size += bp->s.size;
    p->s.ptr = bp->s.ptr;
  } else
    p->s.ptr = bp;
  freep = p;
}

static Header *
morecore(uint nu) {
  char *p;
  Header *hp;

  if (nu * sizeof(Header) < PGSIZE)
    nu = PGSIZE / sizeof(Header);
  else
    return NULL;

  p = kalloc();
  if (p == NULL)
    return NULL;
  hp = (Header *) p;
  hp->s.size = nu;
  kmallocfree((void *) (hp + 1));
  return freep;
}

void *
kmalloc(uint nbytes) {
  Header *p, *prevp;
  uint nunits;

  nunits = (nbytes + sizeof(Header) - 1) / sizeof(Header) + 1;
  if ((prevp = freep) == NULL) {
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }
  for (p = prevp->s.ptr;; prevp = p, p = p->s.ptr) {
    if (p->s.size >= nunits) {
      if (p->s.size == nunits)
        prevp->s.ptr = p->s.ptr;
      else {
        p->s.size -= nunits;
        p += p->s.size;
        p->s.size = nunits;
      }
      freep = prevp;
      return (void *) (p + 1);
    }
    if (p == freep)
      if ((p = morecore(nunits)) == NULL)
        return NULL;
  }
}



