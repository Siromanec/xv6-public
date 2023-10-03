# xv6-syscall
- Виконував: Іванов Сергій https://github.com/Siromanec
- Додаткові: логування сисвикликів і дані про ситему.

### syscall.h
Добавляю номер сисвиклику, щоб його можна було викликати з таблиці сисвикликів.
### syscall.c
Добавляю в таблицю сисвикликів сисвиклик, щоб можна було до функції доступатися за номером (це спрощує роботу з аргументами в асемблері (я про функцію, як аргумент)), оголошую функцію зовнішньою, щоб з іншого файлу можна було підтягнути
### sysfile.c/sysproc.c
Пишу тіло функції, дещо дивним способом дістаю аргументи (сама функція приймає войд, тому треба скористуватися argptr)
### user.h
Оголошую інтерфейс виклику, доступного в користувацькому просторі
### usys.S
Передаю номер сисвиклику у eax. З eax він береться у syscall.c. Викликаємо переривання, яке означає, що зробили сисвиклик. Потім трапгендлер якось це обробляє і викликає syscall() із syscall.c
### printf.c
Добавив підримку доповнення до 9 символів (у числах), uint

## основне

```
$ date
17:58:41 03/10/2023
```

# додаткові

## Логування
`$ tlog` - від toggle logging

Реалізовано глобальною змінною. Її все використання у syscall.c

Приклад виводу
```
 0, 1, ch:32' ',cd ..

)
 sys call read (
 0, 1, ch:99'c',
)
 sys call read (
 0, 1, ch:100'd',
)
 sys call read (
 0, 1, ch:32' ',
)
 sys call read (
 0, 1, ch:46'.',
)
 sys call read (
 0, 1, ch:46'.',
)
 sys call chdir (
 6787, '..',
)
 sys call write (
 2, 1, ch:36'$',$
)
 sys call write (
 2, 1, ch:32' ',
)
 sys call read (
 0, 1, ch:32' ',

```
## стан системи
`$ state`
Виводить дві таблиці: про процеси і процесор, на якому виконується процес (у стані running)

```
$ benchmark&
$ benchmark&
$ benchmark&
$ state
pid: 1  state:sleep     name:    init   memory:    12288        nfiles:03       inodes:( 26, 26, 26,)
pid: 2  state:sleep     name:    sh     memory:    16384        nfiles:03       inodes:( 26, 26, 26,)
pid:27  state:run       name:    state  memory:    45056        nfiles:03       inodes:( 26, 26, 26,)
pid:22  state:runble    name:    benchmark      memory:    12288        nfiles:03       inodes:( 26, 26, 26,)
pid:24  state:runble    name:    benchmark      memory:    12288        nfiles:03       inodes:( 26, 26, 26,)
pid:26  state:run       name:    benchmark      memory:    12288        nfiles:03       inodes:( 26, 26, 26,)
cpu:0   pid:26
cpu:1   pid:27
$ 

```
- benchmark - dummy-програма, що довго виконується
- procdumpWrite - у файлі "proc.c"




# чуже
This is a fork of the original x86 version of MIT's xv6 operating system
(see https://github.com/mit-pdos/xv6-public/), intended for use by the
Faculty of Applied Sciences of the Ukrainian Catholic University (UCU)
in the OS course. This repository is maintained by faculty and students
to improve xv6 and provide compatibility updates.

xv6 is a re-implementation of Dennis Ritchie's and Ken Thompson's Unix
Version 6 (v6).  xv6 loosely follows the structure and style of v6,
but is implemented for a modern x86-based multiprocessor using ANSI C.

ACKNOWLEDGMENTS

xv6 is inspired by John Lions's Commentary on UNIX 6th Edition (Peer
to Peer Communications; ISBN: 1-57398-013-7; 1st edition (June 14,
2000)). See also https://pdos.csail.mit.edu/6.828/, which
provides pointers to on-line resources for v6.

xv6 borrows code from the following sources:
    JOS (asm.h, elf.h, mmu.h, bootasm.S, ide.c, console.c, and others)
    Plan 9 (entryother.S, mp.h, mp.c, lapic.c)
    FreeBSD (ioapic.c)
    NetBSD (console.c)

The following people have made contributions: Russ Cox (context switching,
locking), Cliff Frey (MP), Xiao Yu (MP), Nickolai Zeldovich, and Austin
Clements.

We are also grateful for the bug reports and patches contributed by Silas
Boyd-Wickizer, Anton Burtsev, Cody Cutler, Mike CAT, Tej Chajed, eyalz800,
Nelson Elhage, Saar Ettinger, Alice Ferrazzi, Nathaniel Filardo, Peter
Froehlich, Yakir Goaron,Shivam Handa, Bryan Henry, Jim Huang, Alexander
Kapshuk, Anders Kaseorg, kehao95, Wolfgang Keller, Eddie Kohler, Austin
Liew, Imbar Marinescu, Yandong Mao, Matan Shabtay, Hitoshi Mitake, Carmi
Merimovich, Mark Morrissey, mtasm, Joel Nider, Greg Price, Ayan Shafqat,
Eldar Sehayek, Yongming Shen, Cam Tenny, tyfkda, Rafael Ubal, Warren
Toomey, Stephen Tu, Pablo Ventura, Xi Wang, Keiichi Watanabe, Nicolas
Wolovick, wxdao, Grant Wu, Jindong Zhang, Icenowy Zheng, and Zou Chang Wei.

The code in the files that constitute xv6 is
Copyright 2006-2018 Frans Kaashoek, Robert Morris, and Russ Cox.

ERROR REPORTS

We don't process error reports (see note on top of this file).

BUILDING AND RUNNING XV6

To build xv6 on an x86 ELF machine (like Linux or FreeBSD), run
"make". On non-x86 or non-ELF machines (like OS X, even on x86), you
will need to install a cross-compiler gcc suite capable of producing
x86 ELF binaries (see https://pdos.csail.mit.edu/6.828/).
Then run "make TOOLPREFIX=i386-jos-elf-". Now install the QEMU PC
simulator and run "make qemu".
