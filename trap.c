#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "debug.h"
// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  int pgflt_success = FALSE;
  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);


    }
//    else if(cpuid() == 1){
//      acquire(&tickslock);
//
//      if (ticks !=0 && (ticks % SWAP_INTERVAL) == 0){
//        swap();
//      }
//      release(&tickslock);
//
//    }
    lapiceoi();

      break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    // isn't it equal to T_SYSCALL?
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  //TODO figure out accessing to which address caused a pagefault DONE
    case T_PGFLT:
      ;
      uint addr = rcr2();
#ifdef DEBUG_T_PGFLT
      cprintf("pid %d %s: trap %d err %d on cpu %d "
              "eip 0x%x addr 0x%x -- trying to handle\n",
              myproc()->pid, myproc()->name, tf->trapno,
              tf->err, cpuid(), tf->eip, addr);
#endif
//      int success = FALSE;
      if (handle_pagefault(addr, tf->err) == 0) {
#ifdef DEBUG_T_PGFLT
        cprintf("success\n");
#endif
//        flush_tlb();
        return;
      }
//      if (swaprestore(addr) == 0) { // swaprestore should be first because it is less broad than lazyalloc
//#ifdef DEBUG_T_PGFLT
//        cprintf("restored swapped\n");
//#endif
//        return;
//      }
//      if (lazyalloc(addr) == 0) {
//#ifdef DEBUG_T_PGFLT
//        cprintf("lazily allocated mem\n");
//#endif
//        return;
//      }

//      if (success)
//        return;

      goto ERROR_GOTO;
    case T_DBLFLT:
      cprintf("unexpected trap %d from cpu %d eip 0x%x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("DOUBLE FAULT!\n");
//      break;


ERROR_GOTO:
  default:
    if(myproc() == NULL || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip 0x%x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = TRUE;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
      (tf->trapno == T_IRQ0+IRQ_TIMER || pgflt_success))
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
