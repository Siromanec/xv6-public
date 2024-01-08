#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "debug.h"
#include "swap.h"
#include "iterator.h"

extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;
extern struct {
  struct spinlock lock;
  int use_lock;
  page_data_t data[PHYSTOP / PGSIZE];
} phys_page_data;
extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

int copy_on_write(void *va, pte_t *pte, pde_t *pgdir);

int swaprestore(void *va, pte_t *pte, pde_t *pgdir);
// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.

void
seginit(void) {
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X | STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X | STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, BOOL alloc) {
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if (*pde & PTE_P) {
//      if(*pde & PTE_A)
//        cprintf( "a\n");
    pgtab = (pte_t *) P2V(PTE_ADDR(*pde));
  } else {
    if (!alloc || (pgtab = (pte_t *) kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm) {
  char *a, *last;
  pte_t *pte;

  a = (char *) PGROUNDDOWN((uint) va);
  last = (char *) PGROUNDDOWN(((uint) va) + size - 1);
  for (;;) {
    if ((pte = walkpgdir(pgdir, a, TRUE)) == 0)
      return -1;
    if ((*pte & PTE_P) && !(*pte & PTE_C) && !(*pte & PTE_S)) // if it is a copy on write or swap it is not a remap
      panic("remap");
    *pte = pa | perm | PTE_P;
    if (a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
    {(void *) KERNBASE, 0,             EXTMEM,  PTE_W}, // I/O space
    {(void *) KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
    {(void *) data,     V2P(data),     PHYSTOP, PTE_W}, // kern data+memory
    {(void *) DEVSPACE, DEVSPACE, 0,            PTE_W}, // more devices
};

// Set up kernel part of a page table.
pde_t *
setupkvm(void) {
  pde_t *pgdir;
  struct kmap *k;

  if ((pgdir = (pde_t *) kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
  if (P2V(PHYSTOP) > (void *) DEVSPACE)
    panic("PHYSTOP too high");
  for (k = kmap; k < &kmap[NELEM(kmap)];
  k++)
  if (mappages(pgdir, k->virt, k->phys_end - k->phys_start,
               (uint) k->phys_start, k->perm) < 0) {
    freevm(pgdir);
    return 0;
  }
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void) {
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void) {
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p) {
  if (p == 0)
    panic("switchuvm: no process");
  if (p->kstack == 0)
    panic("switchuvm: no kstack");
  if (p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                sizeof(mycpu()->ts) - 1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint) p->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
  lcr3(V2P(p->pgdir));  // switch to process's address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz) {
  char *mem;

  if (sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W | PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz) {
  uint i, pa, n;
  pte_t *pte;

  if ((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for (i = 0; i < sz; i += PGSIZE) {
    if ((pte = walkpgdir(pgdir, addr + i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if (sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if (readi(ip, P2V(pa), offset + i, n) != n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz) {
  char *mem;
  uint a;

  if (newsz >= KERNBASE)
    return 0;
  if (newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for (; a < newsz; a += PGSIZE) {
    mem = kalloc();
    if (mem == 0) {
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if (mappages(pgdir, (char *) a, PGSIZE, V2P(mem), PTE_W | PTE_U) < 0) {
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz) {
  pte_t *pte;
  uint a, pa;

  if (newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for (; a < oldsz; a += PGSIZE) {
    pte = walkpgdir(pgdir, (char *) a, FALSE);
    if (!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
    else if ((*pte & PTE_P) != 0) {
      if (!(*pte & PTE_S)) {
        pa = PTE_ADDR(*pte);
        if (pa == NULL)
          panic("deallocuvm");
        char *v = P2V(pa);
        kfree(v);
      }
      //TODO free the swapped block
      *pte = 0;
    }
  }
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir) {
  uint i;


  // TODO invalidate swapmap here and swapfile, perhaps in a form of one big transaction
  if (pgdir == NULL)
    panic("freevm: no pgdir");
  deallocuvm(pgdir, KERNBASE, 0);
  for (i = 0; i < NPDENTRIES; i++) {
    if (pgdir[i] & PTE_P) {
      char *v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char *) pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva) {
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if (pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t *
copyuvm(pde_t *pgdir, uint sz) {
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
//  char *mem;

  if ((d = setupkvm()) == NULL)
    return 0;
  for (i = PGSIZE; i < sz; i += PGSIZE) {
    if ((pte = walkpgdir(pgdir, (void *) i, 0)) == NULL)
      panic("copyuvm: pte should exist");
    if (*pte & PTE_S) {
      swaprestore((void *) i, pte, pgdir);
    }//TODO handle this case; possible solution copy all swap entries
    if (!(*pte & PTE_P))
      continue; // TODO remove continue when swap works
//      panic("copyuvm: page not present"); //TODO check for size

//    if ((*pte & PTE_W)) // only pages one can write to must be marked PTE_C
    *pte = (*pte & ~PTE_W) | PTE_C; // change parent's and child's flags
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);// & ~PTE_W) | PTE_C; // copy-on-write child

//    if ((mem = kalloc()) == 0)
//      goto bad;
//    memmove(mem, (char *) P2V(pa), PGSIZE);
    if (mappages(d, (void *) i, PGSIZE, pa, flags) < 0) {
//      kfree(mem);
      goto bad;
    }
//    cprintf("i'm a little troublemaker\n");
    inc_ref_pa(pa); // no errors occured

    flush_tlb();// loads parent tlb

  }
  return d;

  bad:
  freevm(d);
  return 0;
}

//PAGEBREAK!
// Map user virtual address to kernel address.
char *
uva2ka(pde_t *pgdir, char *uva) {
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, FALSE);
//  if ((*pte & PTE_C)){
//    cprintf("trying to cow\n");
//    if(copy_on_write(uva, pgdir) != 0)
//      return 0;
//
//    cprintf("fin cow\n");
//  }


  if ((*pte & PTE_P) == 0)
    return NULL;
  if ((*pte & PTE_U) == 0)
    return NULL;

  if (*pte & PTE_S) {
    if (swaprestore((void *) uva, pte, pgdir) < 0)
      return NULL;
  }
  if ((*pte & PTE_C)) {
    if (copy_on_write(uva, pte, pgdir) < 0)
      return NULL;
    flush_tlb();
  }
  return (char *) P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len) {
  char *buf, *pa0;
  uint n, va0;

  buf = (char *) p;
  while (len > 0) {
    va0 = (uint) PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char *) va0);
    if (pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if (n > len)
      n = len;
#ifdef DEBUG_COPYOUT
    cprintf("trying to copy arguments\n");
#endif
    memmove(pa0 + (va - va0), buf, n);
#ifdef DEBUG_COPYOUT
    cprintf("copied arguments\n");
#endif
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

//#include "file.h"
extern char end[];

int swap() {
  struct proc *p;

  int accessed_page_num = 0;
  int present_page_num = 0;
  static int swapped_page_num = 0;

//  int *pt = (int*) 940129401;
//  int was_holding = FALSE;
//  if(!holding(&ptable.lock))
//    acquire(&ptable.lock);
//  else{
//    was_holding = TRUE;
//  }
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->state == UNUSED)// || p->pid == myproc()->pid)
      continue;

    for (uint a = 0;
         a < PGROUNDUP(p->sz); a += PGSIZE) { // swapping up to program size should be faster// swapping kernel space would be bad

      pte_t *pte = walkpgdir(p->pgdir, (void *) a, 0); //returns number of allocated pde entries

      if (pte == NULL) {
        continue;
      }

      if ((*pte & PTE_P) && !(*pte & PTE_C)) {
        if ((*pte & PTE_A)) {
          *pte &= ~PTE_A;
          ++accessed_page_num;
        } else {
          char *va = (P2V(PTE_ADDR(*pte)));
//          if (va >= end) {
//          *pte &= PTE_FLAGS(*pte);
          *pte &= ~PTE_P; // must be set to not present to cause a pagefault
          *pte |= PTE_S;
#ifdef DEBUG_SWAP
          cprintf("swap: swapping proc: %s va: 0x%x\n", p->name, a);
#endif
          swapwrite(va, (void *) a);

          kfree(va); //TODO
          swapped_page_num++;
//          } // clearing physical address to avoid anomalies

        }

        ++present_page_num;
      }
    }

  }
//  if(!was_holding)
//    release(&ptable.lock);

#ifdef DEBUG_SWAP
  cprintf("swap: accessed pages: %d\tpresent pages: %d\tswappedpages:%d\n", accessed_page_num, present_page_num,
          swapped_page_num);
#endif

  return 0;
}

/*
 * @va - virtual address that is being swapped
 * @pte - pointer to
 * @pgdir - page directory
 * */
int swaprestore(void *va, pte_t *pte, pde_t *pgdir) {


#ifdef DEBUG_SWAPRESTORE
  //    cprintf("swaprestore: raw_va: 0x%x\n", raw_va);
    cprintf("swaprestore: va: 0x%x\n", va);
    cprintf("swaprestore: *pte: 0x%x\n", *pte);
    cprintf("swaprestore: PRESENT!\n");
    cprintf("swaprestore: pre: 0x%x\n", (PTE_ADDR(*pte)));
#endif

  char *pa = swapread(va, PTE_ADDR(*pte));
  *pte &= (~PTE_S);
  uint flags = PTE_FLAGS(*pte);
  if (mappages(pgdir, (char *) va, PGSIZE, V2P(pa), flags) < 0) {
    cprintf("swaprestore out of memory (1)\n");
    kfree(pa);
    return -1;
  }
#ifdef DEBUG_SWAPRESTORE
  cprintf("swaprestore: post: 0x%x\n", PTE_ADDR(*pte));
#endif
  return 0;
}

int lazyalloc(void *va, struct proc *p) {

  // there was a check like in swap for kernbase, but idk why


  char *mem;
  mem = kalloc();
  if (mem == 0) {
    cprintf("lazyalloc out of memory\n");
    return -1;
  }
  memset(mem, 0, PGSIZE);
  if (mappages(p->pgdir, (char *) va, PGSIZE, V2P(mem), PTE_W | PTE_U) < 0) {
    cprintf("lazyalloc out of memory (2)\n");
    kfree(mem);
    return -1;
  }
  return 0;
}

int copy_on_write(void *va, pte_t *pte, pde_t *pgdir) {

  char *mem;
  uint pa_to_free = PTE_ADDR(*pte);
  int refcount = get_ref_pa(pa_to_free);
  if (refcount == 1) {
#ifdef DEBUG_COW
    cprintf("making last copy 0x%x\n", pa_to_free);
    cprintf ("Caller name: 0x%x\n", __builtin_return_address(0));
    cprintf("isCow?: 0b%b\n", !!(PTE_FLAGS(*pte)&PTE_C));
    cprintf("pre: 0b%b\n", PTE_FLAGS(*pte));
#endif
    *pte &= ~PTE_C;
    *pte |= PTE_W;
#ifdef DEBUG_COW
    cprintf("post: 0b%b\n", PTE_FLAGS(*pte));
#endif
    return 0;
  }
  mem = kalloc();
  if (mem == NULL) {
    cprintf("copy_on_write out of memory\n");
    return -1;
  }
  memmove(mem, P2V(pa_to_free), PGSIZE);

  if (mappages(pgdir, (char *) va, PGSIZE, V2P(mem), PTE_W | PTE_U) < 0) {
    cprintf("copy_on_write out of memory (2)\n");
    kfree(mem);
    return -1;
  }
  *pte &= ~PTE_C;
  dec_ref_pa(pa_to_free);
  return 0;
};

int handle_pagefault(uint addr, uint err) {
  struct proc *p = myproc();
  uint raw_va = addr;
  void *va = (void *) PGROUNDDOWN(raw_va);

  pte_t *pte;


  if (va == NULL) {
    cprintf("NULL dereferencing\n");
    return -1;
  }
  if ((uint) va >= KERNBASE) // kernel mustn't generate pagefault
    return -1;

  if ((uint) va > p->sz) { // actual size is shifted
    cprintf("address 0x%x bigger than size 0x%x\n", va, p->sz);
    return -1;
  }

  if ((pte = walkpgdir(p->pgdir, va, FALSE)) == NULL) {
#ifdef DEBUG_T_PGFLT
    cprintf("trying to lazyalloc");
#endif
    return lazyalloc(va, p);
  }
  int result = -1;
  if ((*pte & PTE_S)) { // it was a presence error
#ifdef DEBUG_T_PGFLT
    cprintf("trying to restore the swapped page\n");
#endif
    result = MAX(result, swaprestore(va, pte, p->pgdir));
  }
  if ((*pte & PTE_C) && (err & PTE_W)) { // ensure that it was just write error
#ifdef DEBUG_T_PGFLT
    cprintf("trying to copy_on_write\n");
#endif
    result = MAX(result, copy_on_write(va, pte, p->pgdir));
//    if(result == 0) {
//      dec_ref_pa(mappages(pte))
//    }
  }
  if (result != 0) {
#ifdef DEBUG_T_PGFLT
    cprintf("trying to lazyalloc (2)\n");
#endif
    return lazyalloc(va, p);
  }
  //flush tlb because PTEs change
  return result;

//  return 0;
}

inline void
flush_tlb(void) {
  lcr3(V2P(myproc()->pgdir));
}

//int random_swap();
//int lru_swap();
//int fifo_swap();
//
//int swapvictim() {
//#ifdef SWAP_FIFO
//  return fifo_swap();
//#endif
//  return 0;
//}

typedef struct pa_pte_iterator {
  iterator_t iterator;
  page_data_t pd; // never use ptr; is modified by iterator
  uint pa;
} pa_pte_iterator_t;

void pa_pte_iterator_init(pa_pte_iterator_t *iterator, const page_data_t * const pd, uint pa) {

  if (iterator == NULL || pd == NULL)
    return;

  iterator->pa = pa;
  if ((uint) pd->ref_count == 1) {
    iterator->iterator.item_size = 0;
    iterator->iterator.next_item = NULL;
    iterator->iterator.end = (void *) NULL + sizeof(struct proc);
//    iterator->pd = NULL;
  } else {
    iterator->iterator.item_size = sizeof(struct proc);
    iterator->iterator.end = ptable.proc + sizeof(ptable.proc) + sizeof(struct proc);
    iterator->iterator.next_item = ptable.proc; /* TODO pointer to first used process in ptable*/
    iterator->pd = *pd;
  }
};


BOOL pa_pte_iterator_has_next(pa_pte_iterator_t *iterator) {
  return iterator->pd.ref_count != 0 && iterator_has_next((iterator_t *)iterator, NULL);
};

void * pa_pte_iterator_get_next(pa_pte_iterator_t *iterator) {
  struct proc *p;
  pte_t *pte;

  if (iterator->iterator.next_item == NULL) {
    iterator->iterator.next_item = iterator->iterator.end;
    iterator->pd.ref_count--;

    return iterator->pd.pte;
  }
//  acquire(&ptable.lock);
  p = (struct proc *) iterator->iterator.next_item;
  for (; p != iterator->iterator.end; p += sizeof(struct proc)) {
    if (p->state == UNUSED)
      continue;
    pte = walkpgdir(p->pgdir, (void *) iterator->pd.va, FALSE);

    if (PTE_ADDR(pte) == iterator->pa) {
      iterator->iterator.next_item = p + sizeof(struct proc);
      iterator->pd.ref_count--;
      return pte;
    }
  }
//  release(&ptable.lock);


  return NULL;
};


void interract_with_pte(int (*callback)(pte_t *)){
  pa_pte_iterator_t iterator;
  page_data_t pd = {NULL, {NULL}};
  pa_pte_iterator_init(&iterator, &pd, NULL);

  while (pa_pte_iterator_has_next(&iterator)) {
    pte_t * pte = pa_pte_iterator_get_next(&iterator);
    callback(pte);
  }
}