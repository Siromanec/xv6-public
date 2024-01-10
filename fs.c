// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines.  The (higher-level) system call implementations
// are in sysfile.c.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"
#include "memlayout.h"
#include "swap.h"
#include "debug.h"
#include "unordered_map.h"
#include "iterator.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

static void itrunc(struct inode *);

struct swap_s;
//struct spinlock;

// there should be one superblock per disk device, but we run with
// only one device
struct superblock sb;
struct {
  struct spinlock lock;
  struct swap_s buf[512 / SWBLOCKS];
  struct swap_s head;
} swapll;

// Read the super block.
void
readsb(int dev, struct superblock *sb) {
  struct buf *bp;

  bp = bread(dev, 1);
  memmove(sb, bp->data, sizeof(*sb));
  brelse(bp);
}

// Zero a block.
static void
bzero(int dev, int bno) {
  struct buf *bp;

  bp = bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  log_write(bp);
  brelse(bp);
}

// Blocks.

// Allocate a zeroed disk block.
static uint
balloc(uint dev) {
  int b, bi, m;
  struct buf *bp;

  bp = NULL;
  for (b = 0; b < sb.size; b += BPB) {
    bp = bread(dev, BBLOCK(b, sb));
    for (bi = 0; bi < BPB && b + bi < sb.size; bi++) {
      m = 1 << (bi % 8);
      if ((bp->data[bi / 8] & m) == 0) {  // Is block free?
        bp->data[bi / 8] |= m;  // Mark block in use.
        log_write(bp);
        brelse(bp);
        bzero(dev, b + bi);
        return b + bi;
      }
    }
    brelse(bp);
  }
  panic("balloc: out of blocks");
}

// Free a disk block.
static void
bfree(int dev, uint b) {
  struct buf *bp;
  int bi, m;

  bp = bread(dev, BBLOCK(b, sb));
  bi = b % BPB;
  m = 1 << (bi % 8);
  if ((bp->data[bi / 8] & m) == 0)
    panic("freeing free block");
  bp->data[bi / 8] &= ~m;
  log_write(bp);
  brelse(bp);
}

// Inodes.
//
// An inode describes a single unnamed file.
// The inode disk structure holds metadata: the file's type,
// its size, the number of links referring to it, and the
// list of blocks holding the file's content.
//
// The inodes are laid out sequentially on disk at
// sb.startinode. Each inode has a number, indicating its
// position on the disk.
//
// The kernel keeps a cache of in-use inodes in memory
// to provide a place for synchronizing access
// to inodes used by multiple processes. The cached
// inodes include book-keeping information that is
// not stored on disk: ip->ref and ip->valid.
//
// An inode and its in-memory representation go through a
// sequence of states before they can be used by the
// rest of the file system code.
//
// * Allocation: an inode is allocated if its type (on disk)
//   is non-zero. ialloc() allocates, and iput() frees if
//   the reference and link counts have fallen to zero.
//
// * Referencing in cache: an entry in the inode cache
//   is free if ip->ref is zero. Otherwise ip->ref tracks
//   the number of in-memory pointers to the entry (open
//   files and current directories). iget() finds or
//   creates a cache entry and increments its ref; iput()
//   decrements ref.
//
// * Valid: the information (type, size, &c) in an inode
//   cache entry is only correct when ip->valid is 1.
//   ilock() reads the inode from
//   the disk and sets ip->valid, while iput() clears
//   ip->valid if ip->ref has fallen to zero.
//
// * Locked: file system code may only examine and modify
//   the information in an inode and its content if it
//   has first locked the inode.
//
// Thus a typical sequence is:
//   ip = iget(dev, inum)
//   ilock(ip)
//   ... examine and modify ip->xxx ...
//   iunlock(ip)
//   iput(ip)
//
// ilock() is separate from iget() so that system calls can
// get a long-term reference to an inode (as for an open file)
// and only lock it for short periods (e.g., in read()).
// The separation also helps avoid deadlock and races during
// pathname lookup. iget() increments ip->ref so that the inode
// stays cached and pointers to it remain valid.
//
// Many internal file system functions expect the caller to
// have locked the inodes involved; this lets callers create
// multi-step atomic operations.
//
// The icache.lock spin-lock protects the allocation of icache
// entries. Since ip->ref indicates whether an entry is free,
// and ip->dev and ip->inum indicate which i-node an entry
// holds, one must hold icache.lock while using any of those fields.
//
// An ip->lock sleep-lock protects all ip-> fields other than ref,
// dev, and inum.  One must hold ip->lock in order to
// read or write that inode's ip->valid, ip->size, ip->type, &c.

struct {
  struct spinlock lock;
  struct inode inode[NINODE];
} icache;

void
iinit(int dev) {
  int i = 0;

  initlock(&icache.lock, "icache");
  for (i = 0; i < NINODE; i++) {
    initsleeplock(&icache.inode[i].lock, "inode");
  }

  readsb(dev, &sb);
  cprintf("sb: size %d nblocks %d ninodes %d nlog %d logstart %d\
 inodestart %d bmap start %d swap start %d\n", sb.size, sb.nblocks,
          sb.ninodes, sb.nlog, sb.logstart, sb.inodestart,
          sb.bmapstart, sb.swapstart);
}

static struct inode *iget(uint dev, uint inum);

//PAGEBREAK!
// Allocate an inode on device dev.
// Mark it as allocated by  giving it type type.
// Returns an unlocked but allocated and referenced inode.
struct inode *
ialloc(uint dev, short type) {
  int inum;
  struct buf *bp;
  struct dinode *dip;
  cprintf("%d %d", dev, type);
  for (inum = 1; inum < sb.ninodes; inum++) {
    bp = bread(dev, IBLOCK(inum, sb));
    dip = (struct dinode *) bp->data + inum % IPB;
    if (dip->type == 0) {  // a free inode
      memset(dip, 0, sizeof(*dip));
      dip->type = type;
      log_write(bp);   // mark it allocated on the disk
      brelse(bp);
      return iget(dev, inum);
    }
    brelse(bp);
  }
  panic("ialloc: no inodes");
}

// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk, since i-node cache is write-through.
// Caller must hold ip->lock.
void
iupdate(struct inode *ip) {
  struct buf *bp;
  struct dinode *dip;

  bp = bread(ip->dev, IBLOCK(ip->inum, sb));
  dip = (struct dinode *) bp->data + ip->inum % IPB;
  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  log_write(bp);
  brelse(bp);
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
static struct inode *
iget(uint dev, uint inum) {
  struct inode *ip, *empty;

  acquire(&icache.lock);

  // Is the inode already cached?
  empty = 0;
  for (ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++) {
    if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {
      ip->ref++;
      release(&icache.lock);
      return ip;
    }
    if (empty == 0 && ip->ref == 0)    // Remember empty slot.
      empty = ip;
  }

  // Recycle an inode cache entry.
  if (empty == 0)
    panic("iget: no inodes");

  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->valid = 0;
  release(&icache.lock);

  return ip;
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode *
idup(struct inode *ip) {
  acquire(&icache.lock);
  ip->ref++;
  release(&icache.lock);
  return ip;
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void
ilock(struct inode *ip) {
  struct buf *bp;
  struct dinode *dip;

  if (ip == NULL || ip->ref < 1)
    panic("ilock");

  acquiresleep(&ip->lock);

  if (ip->valid == 0) {
    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode *) bp->data + ip->inum % IPB;
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    brelse(bp);
    ip->valid = 1;
    if (ip->type == 0)
      panic("ilock: no type");
  }
}

// Unlock the given inode.
void
iunlock(struct inode *ip) {
  if (ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
    panic("iunlock");

  releasesleep(&ip->lock);
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode cache entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
void
iput(struct inode *ip) {
  acquiresleep(&ip->lock);
  if (ip->valid && ip->nlink == 0) {
    acquire(&icache.lock);
    int r = ip->ref;
    release(&icache.lock);
    if (r == 1) {
      // inode has no links and no other references: truncate and free.
      itrunc(ip);
      ip->type = 0;
      iupdate(ip);
      ip->valid = 0;
    }
  }
  releasesleep(&ip->lock);

  acquire(&icache.lock);
  ip->ref--;
  release(&icache.lock);
}

// Common idiom: unlock, then put.
void
iunlockput(struct inode *ip) {
  iunlock(ip);
  iput(ip);
}

//PAGEBREAK!
// Inode content
//
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[].  The next NINDIRECT blocks are
// listed in block ip->addrs[NDIRECT].

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
static uint
bmap(struct inode *ip, uint bn) {
  uint addr, *a;
  struct buf *bp;

  if (bn < NDIRECT) {
    if ((addr = ip->addrs[bn]) == NULL)
      ip->addrs[bn] = addr = balloc(ip->dev);
    return addr;
  }
  bn -= NDIRECT;

  if (bn < NINDIRECT) {
    // Load indirect block, allocating if necessary.
    if ((addr = ip->addrs[IND_BLOCK]) == NULL)
      ip->addrs[IND_BLOCK] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint *) bp->data;
    if ((addr = a[bn]) == NULL) {
      a[bn] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }

  bn -= NINDIRECT;


  panic("bmap: out of range");
}

// Truncate inode (discard contents).
// Only called when the inode has no links
// to it (no directory entries referring to it)
// and has no in-memory reference to it (is
// not an open file or current directory).
static void
itrunc(struct inode *ip) {
  int i, j;
  struct buf *bp;
  uint *a;

  for (i = 0; i < NDIRECT; i++) {
    if (ip->addrs[i]) {
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  if (ip->addrs[NDIRECT]) {
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint *) bp->data;
    for (j = 0; j < NINDIRECT; j++) {
      if (a[j])
        bfree(ip->dev, a[j]);
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  ip->size = 0;
  iupdate(ip);
}

// Copy stat information from inode.
// Caller must hold ip->lock.
void
stati(struct inode *ip, struct stat *st) {
  st->dev = ip->dev;
  st->ino = ip->inum;
  st->type = ip->type;
  st->nlink = ip->nlink;
  st->size = ip->size;
}

//PAGEBREAK!
// Read data from inode.
// Caller must hold ip->lock.
int
readi(struct inode *ip, char *dst, uint off, uint n) {
  uint tot, m;
  struct buf *bp;

  if (ip->type == T_DEV) {
    if (ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].read)
      return -1;
    return devsw[ip->major].read(ip, dst, n);
  }

  if (off > ip->size || off + n < off)
    return -1;
  if (off + n > ip->size)
    n = ip->size - off;

  for (tot = 0; tot < n; tot += m, off += m, dst += m) {
    bp = bread(ip->dev, bmap(ip, off / BSIZE));
    m = min(n - tot, BSIZE - off % BSIZE);
    memmove(dst, bp->data + off % BSIZE, m);
    brelse(bp);
  }
  return n;
}

// PAGEBREAK!
// Write data to inode.
// Caller must hold ip->lock.
int
writei(struct inode *ip, char *src, uint off, uint n) {
  uint tot, m;
  struct buf *bp;

  if (ip->type == T_DEV) {
    if (ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].write)
      return -1;
    return devsw[ip->major].write(ip, src, n);
  }

  if (off > ip->size || off + n < off)
    return -1;
  if (off + n > MAXFILE * BSIZE)
    return -1;

  for (tot = 0; tot < n; tot += m, off += m, src += m) {
    bp = bread(ip->dev, bmap(ip, off / BSIZE));
    m = min(n - tot, BSIZE - off % BSIZE);
    memmove(bp->data + off % BSIZE, src, m);
    log_write(bp);
    brelse(bp);
  }

  if (n > 0 && off > ip->size) {
    ip->size = off;
    iupdate(ip);
  }
  return n;
}

//PAGEBREAK!
// Directories

int
namecmp(const char *s, const char *t) {
  return strncmp(s, t, DIRSIZ);
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode *
dirlookup(struct inode *dp, char *name, uint *poff) {
  uint off, inum;
  struct dirent de;

  if (dp->type != T_DIR)
    panic("dirlookup not DIR");

  for (off = 0; off < dp->size; off += sizeof(de)) {
    if (readi(dp, (char *) &de, off, sizeof(de)) != sizeof(de))
      panic("dirlookup read");
    if (de.inum == 0)
      continue;
    if (namecmp(name, de.name) == 0) {
      // entry matches path element
      if (poff)
        *poff = off;
      inum = de.inum;
      return iget(dp->dev, inum);
    }
  }

  return 0;
}

// Write a new directory entry (name, inum) into the directory dp.
int
dirlink(struct inode *dp, char *name, uint inum) {
  int off;
  struct dirent de;
  struct inode *ip;

  // Check that name is not present.
  if ((ip = dirlookup(dp, name, 0)) != 0) {
    iput(ip);
    return -1;
  }

  // Look for an empty dirent.
  for (off = 0; off < dp->size; off += sizeof(de)) {
    if (readi(dp, (char *) &de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if (de.inum == 0)
      break;
  }

  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if (writei(dp, (char *) &de, off, sizeof(de)) != sizeof(de))
    panic("dirlink");

  return 0;
}

//PAGEBREAK!
// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char *
skipelem(char *path, char *name) {
  char *s;
  int len;

  while (*path == '/')
    path++;
  if (*path == 0)
    return 0;
  s = path;
  while (*path != '/' && *path != 0)
    path++;
  len = path - s;
  if (len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while (*path == '/')
    path++;
  return path;
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
static struct inode *
namex(char *path, int nameiparent, char *name) {
  struct inode *ip, *next;

  if (*path == '/')
    ip = iget(ROOTDEV, ROOTINO);
  else
    ip = idup(myproc()->cwd);

  while ((path = skipelem(path, name)) != 0) {
    ilock(ip);
    if (ip->type != T_DIR) {
      iunlockput(ip);
      return 0;
    }
    if (nameiparent && *path == '\0') {
      // Stop one level early.
      iunlock(ip);
      return ip;
    }
    if ((next = dirlookup(ip, name, 0)) == 0) {
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    ip = next;
  }
  if (nameiparent) {
    iput(ip);
    return 0;
  }
  return ip;
}

struct inode *
namei(char *path) {
  char name[DIRSIZ];
  return namex(path, 0, name);
}

struct inode *
nameiparent(char *path, char *name) {
  return namex(path, 1, name);
}

struct {
  //first so it is page aligned
  char readbuf[PGSIZE];

  struct spinlock lock;
  struct file *f;
  uint bmap_size;
} swapfile;
extern char end[];
static inline BOOL

swapfile_bitmap_is_used(char value, uint i) {
  return (BOOL) ((value & (1 << (i % 8))) != 0);
};

void swapfile_bitmap_set(BOOL bit, uint i) {

  /* 1. read bitmap
   * 2. | the needed value
   * 3. write changes*/
  char value = 0;
//  cprintf("i = %d\n", i);


  if (fileseek(swapfile.f, i / 8, SEEK_SET) < 0)
    panic("swapfile_bitmap_set: fileseek");
  if (fileread(swapfile.f, (char *) &value, sizeof(value)) < 0)
    panic("swapfile_bitmap_set: fileread");

//  cprintf("read flag value 0b%b\n", value);

  /* 1. get bit match (either 0 or 0*10*)
   * 2. ~ -> (1* or 1*01*)
   * 3. & -> (bit was 0 and stays the same or bit was 1 and is nullified)
   * 4. | with value to set*/
  value &= ~((value & (1 << (i % 8))));
  if (bit)
    value |= (1 << (i % 8));



  if (fileseek(swapfile.f, i / 8, SEEK_SET) < 0)
    panic("swapfile_bitmap_set: fileseek (2)");
  if (filewrite(swapfile.f, (char *) &value, sizeof(value)) < 0)
    panic("swapfile_bitmap_set: filewrite");

//  cprintf("written flag value 0b%b\n", value);

/*  char test;
  if (fileseek(swapfile.f, i / 8, SEEK_SET) < 0)
    panic("swapfile_bitmap_set: fileseek");
  if (fileread(swapfile.f, (char *) &test, sizeof(test)) < 0)
    panic("swapfile_bitmap_set: fileread");*/

//  cprintf("read flag value 0b%b\n", test);

//  cprintf("flag value 0b%b for i = %d\n", test, i);
};

uint swapfile_get_free_page() {
  uint i;
//  acquire(&swapfile.lock);
  fileseek(swapfile.f, 0, SEEK_SET);

  for (i = 0; i < swapfile.bmap_size * 8; ++i) {

    if ((i) % (sizeof(swapfile.readbuf) * 8) == 0) {
      if (fileread(swapfile.f,
                   swapfile.readbuf,
                   min(sizeof(swapfile.readbuf),
                       swapfile.bmap_size - (i / 8))) < 0) {

        panic("swapfile_get_free_page: fileread");
      } /*else {
        cprintf("selected size: %d\n", min(sizeof(swapfile.readbuf),
                                           swapfile.bmap_size - (i / 8)));
      }*/
    }

/*    cprintf("0b%b is used 0b%b for i = %d\n", (int) swapfile.readbuf[(i / 8) % sizeof(swapfile.readbuf)],
            (int) swapfile_bitmap_is_used(swapfile.readbuf[(i / 8) % sizeof(swapfile.readbuf)], i), i);*/

    if (!swapfile_bitmap_is_used(swapfile.readbuf[(i / 8) % sizeof(swapfile.readbuf)], i)) {
      swapfile_bitmap_set(TRUE, i);
      return i;
    }

  }
  panic("swapfile_get_free_page: no free pages left");
};


int swapfile_write_page(void *src, uint i) {
//  if ((swapfile.f->ip->size < swapfile.bmap_size + (i + 1) * PGSIZE))
//    panic("swapfile_write_page: out of range");

  fileseek(swapfile.f, swapfile.bmap_size + (i) * PGSIZE, SEEK_SET);

  filewrite(swapfile.f, src, PGSIZE);
  return 0;

}; // always writes PGSIZE bytes
int swapfile_read_page(void *dst, uint i) {

  if ((swapfile.f->ip->size < swapfile.bmap_size + (i + 1) * PGSIZE) &&
      filetruncate(swapfile.f, swapfile.bmap_size + (i + 1) * PGSIZE) < 0)
    panic("swapfile_read_page: filetruncate");

  fileseek(swapfile.f, swapfile.bmap_size + (i) * PGSIZE, SEEK_SET);

  fileread(swapfile.f, dst, PGSIZE);
  return 0;


}; // always reads PGSIZE bytes

int swapfile_free_page(uint i) {
  static char null_page[PGSIZE];
  swapfile_bitmap_set(FALSE, i);
  swapfile_write_page(null_page, i);
  return 0;
};

int swapfile_use_example() {
  char *va = NULL;
  // TODO add locks and return value checkers
  acquire(&swapfile.lock);
  uint i = swapfile_get_free_page();
  swapfile_write_page(va, i);
  release(&swapfile.lock);

  /* swap out end
   * ...
   * swap in start*/
  acquire(&swapfile.lock);

  swapfile_read_page(va, i);

  swapfile_free_page(i);
  release(&swapfile.lock);

  return 0;
}

//#ifdef SWAPFILE
extern UnorderedMap swapMap;

void swapinit_file(void) {
  /*creating swapfile*/
  struct inode *swapinode;
  struct file *f;

  initlock(&swapfile.lock, "swapfile");

  begin_op();

  if ((swapinode = ialloc(ROOTDEV, T_FILE)) == NULL)
    panic("swapinit: ialloc");
  ilock(swapinode);
  swapinode->major = 0;
  swapinode->minor = 0;
  swapinode->nlink = 1;
  iupdate(swapinode);
  if ((f = filealloc()) == NULL) {
    iunlockput(swapinode);
    end_op();
    panic("swapinit: could not allocate file");
  }
  iunlock(swapinode);
  end_op();
  f->type = FD_INODE;
  f->ip = swapinode;
  f->off = 0;
  f->readable = TRUE;
  f->writable = TRUE;

  swapfile.f = f;

  /* 1. get size of physmem
   * 2. get page count and round up
   * 3. get required size in bytes and round up
   * */
  swapfile.bmap_size = (((uint) (P2V(PHYSTOP) - (uint) end - 1) / PGSIZE) + 1 - 1) / 8 + 1;

  cprintf("swapfile.bmap_size %d\n", swapfile.bmap_size);
  if (filetruncate(f, (off_t) swapfile.bmap_size) < 0)
    panic("swapinit: filetruncate");

  SwapMapInit(&swapMap);

//  swapmap_add_remove_test();


};

//#elifdef SWAPSB
void swapinit_sb(void) {
  struct swap_s *s;


  initlock(&swapll.lock, "swapll");

//PAGEBREAK!
  // Create linked list of buffers
  swapll.head.prev = &swapll.head;
  swapll.head.next = &swapll.head;
  uint ss = sb.swapstart;
//  cprintf("i enter here\n");
//  cprintf("0x%x\t0x%x\n", swapll.buf, swapll.buf + (sb.nswap / 4));
  for (s = swapll.buf; s < swapll.buf + (sb.nswap / SWBLOCKS); s++) {
    s->next = swapll.head.next;
    s->prev = &swapll.head;
//    begin_op();
//    for (int i = 0; i < SWBLOCKS; ++i) {
//      s->block[i] = balloc(ROOTDEV);
//
//    }
//    end_op();
    s->block = ss;
//    cprintf("ss: %d\n", ss);
    s->taken = FALSE;
    ss += (SWBLOCKS);
    initsleeplock(&s->lock, "swap");
    swapll.head.next->prev = s;
    swapll.head.next = s;
  }
}


/**
 * @param buf -- the physical address of the page
 * @param la -- logical address of the page
 * @param pte -- pointer for the page table entry
 * @modifies swapfile; pte flags; swapMap; phys_page_table; frees the memory*/
void swapwrite_file(const char *buf, void *la, pte_t *buf_pte) {

  /*write to file*/
//  acquire(&swapfile.lock); // cant lock while doing disk operations
  uint pageNo = swapfile_get_free_page();
  cprintf("swapwrite_file:  pageno: %d\n", pageNo);


  swapfile_write_page((char *) buf, pageNo);
//  release(&swapfile.lock);

//  uint pageNo = NULL;

  SwapUniqueKey key = {.pa = V2P(buf), .log_a = (uint) la};

  acquire(&swapMap.lock);
  /*get node to which ptes will be written*/

  LinkedListHead *bin = UnorderedMapGetBin(&swapMap, &key);

  LinkedListAdd(bin, &key, NULL);

  LinkedListNode *node = bin->end;

  SwapData *data = node->data;

  data->swapfilePageNo = pageNo;
  LinkedListHead *PTEs = data->PTEs;

  page_data_t *pd = get_pd(V2P(buf));
  cprintf("swapwrite_file: refcnt: %d\n", pd->ref_count);

  if (pd->ref_count == 1) {
    /*&pte because it is the pointer that is important, not the contents*/
    LinkedListAdd(PTEs, &buf_pte, NULL);
    *buf_pte |= PTE_S;
    *buf_pte &= ~PTE_P;
    cprintf("swapwrite_file: write pte* : 0x%x\n", buf_pte);

  } else {
    /*get iterator*/
    pa_pte_iterator_t iterator;
    pa_pte_iterator_init(&iterator, pd, V2P(buf));
    /*iterate over ptes the memory is refering to*/
    while (pa_pte_iterator_has_next(&iterator)) {
      pte_t *pte = pa_pte_iterator_get_next(&iterator);
      LinkedListAdd(PTEs, &pte, NULL);
      *pte |= PTE_S;
      *pte &= ~PTE_P;
      cprintf("swapwrite_file: write pte* : 0x%x\n", pte);

    }
  }


  release(&swapMap.lock);

  reset_and_free_pa_pd(V2P(buf));

  cprintf("swapwrite_file: ended writing\n");
//  flush_tlb();

}



// assumes input buffer is of size PGSIZE
void swapwrite(const char *buf, void *va) {
  struct swap_s *s;

  struct buf *bp;

  if ((uint) (va) % PGSIZE)
    panic("swapwrite va");
//  acquire(&swapll.lock);
//  for (s = swapll.head.next; s != &swapll.head; s = s->next) {
  for (int j = 0; j < (sb.nswap / (SWBLOCKS)); j++) {
    s = &swapll.buf[j];
//    cprintf("hello from loop");
    if (!s->taken) {
//      acquiresleep(&s->lock);

      begin_op();

      for (int i = 0; i < SWBLOCKS; ++i) {
        bp = bread(ROOTDEV, s->block + i);
        memmove(bp->data, buf + i * BSIZE, BSIZE);
        log_write(bp);
        brelse(bp);
      }
      end_op();

      s->va = va;
      s->pa = V2P(buf);
//      releasesleep(&s->lock);
      s->taken = TRUE;

      return;
    }
  }
  panic("swapwrite");

}

/**
 * @param buf -- the physical address of the page
 * @param la -- logical address of the page
 * @param pte -- pointer for the page table entry
 * @modifies swapfile; pte flags; swapMap; phys_page_table; frees the memory*/
void
swapread_file(void *la, pte_t *buf_pte) {
  uint old_pa = PTE_ADDR(*buf_pte);


  SwapUniqueKey key = {.pa = old_pa, .log_a = (uint) la};

  cprintf("swapread_file: old pa: 0x%x\n", old_pa/PGSIZE);

  acquire(&swapMap.lock);
  /*get node to which ptes will be written*/

  LinkedListHead *bin = UnorderedMapGetBin(&swapMap, &key);

//  LinkedListAdd(bin, &key, NULL);

  LinkedListNode *node = LinkedListGet(bin, &key);
  if (node == NULL) {
      panic("swapread_file: swapmap: key not found\n");
  }

  SwapData *data;
  void * new_va = kalloc();
  page_data_t * pd = get_pd(V2P(new_va));
  cprintf("swapread_file: new pa: 0x%x\n", V2P(new_va)/PGSIZE);



  do {
    data = node->data;

    LinkedListNode * fitting_pte_entry = LinkedListGet(data->PTEs, &buf_pte);
    if(fitting_pte_entry == NULL){
      node = LinkedListNodeGetNextMatching(node->next, bin, &key);
      if(node == NULL)
        break;
      continue;
    }
    LinkedListNode * pte_entry = data->PTEs->start;
    pd->ref_count = data->PTEs->length;

    do {

      pte_t * pte = *(pte_t **)(pte_entry->uniqueKey);
      cprintf("swapread_file: pte* : 0x%x\n", pte);
      *pte &= (~PTE_S);
      int flags = PTE_FLAGS(*pte);

      mappage(la, pte, V2P(new_va), flags);

      pte_entry = pte_entry->next;
      if(pte_entry ==  NULL)
        break;

    }
    while (TRUE);
    break;
  }
  while (node != NULL);

  if (node == NULL) {
    panic("swapread_file: did not find pte");
  }

  cprintf("swapread_file : 0x%x\n", data);
  uint pageNo = data->swapfilePageNo;
  cprintf("swapread_file:  pageno: %d\n", pageNo);

  LinkedListNodeRemoveNextMatching(node, bin, NULL);


  release(&swapMap.lock);


//  acquire(&swapfile.lock);
  swapfile_read_page(new_va, pageNo);
  swapfile_free_page(pageNo);
//  release(&swapfile.lock);

  cprintf("swapread_file: ended reading\n");



}

extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

void swapfree_file(char * va, void * la, pte_t * pte) {

  //TODO free the swapped block

  /* 1. find bin
   * 2. find SwapNodeData, which contains pte
   * 3. remove node containing pte
   * 4. if it was the last node (aka PTEs->length == 0), swapfile_free, remove SwapNode*/
//  uint old_pa = PTE_ADDR(*buf_pte);


  SwapUniqueKey key = {.pa = V2P(va), .log_a = (uint) la};

  cprintf("swapfree: old pa: 0x%x\n", V2P(va)/PGSIZE);

  acquire(&swapMap.lock);
  /*get node to which ptes will be written*/

  LinkedListHead *bin = UnorderedMapGetBin(&swapMap, &key);

//  LinkedListAdd(bin, &key, NULL);

  LinkedListNode *node = LinkedListGet(bin, &key);
  if (node == NULL) {
    panic("swapfree: swapmap: key not found\n");
  }

  SwapData *data;

  do {
    data = node->data;

    LinkedListNode * fitting_pte_entry = LinkedListGet(data->PTEs, &pte);
    if(fitting_pte_entry == NULL){
      node = LinkedListNodeGetNextMatching(node->next, bin, &key);
      if(node == NULL)
        break;
      continue;
    }

    LinkedListNodeRemoveNextMatching(fitting_pte_entry, data->PTEs, NULL);
    break;
  }
  while (node != NULL);

  if (node == NULL) {
    panic("swapfree: did not find pte");
  }

  cprintf("swapfree : 0x%x\n", data);
  uint pageNo = data->swapfilePageNo;
  cprintf("swapfree:  pageno: %d\n", pageNo);

  if (data->PTEs->length == 0) {
    LinkedListNodeRemoveNextMatching(node, bin, NULL);
    release(&swapMap.lock);

    release(&ptable.lock);
//    acquire(&swapfile.lock);

    swapfile_free_page(pageNo);
//    release(&swapfile.lock);

    acquire(&ptable.lock);
  } else {
    release(&swapMap.lock);
  }





}

// reads into self-allocated page and returns whatever kalloc returns
// va is rounded down to beginning of the page
void *swapread(void *va, uint pa) {
  //I think its a matter of setting another physical address in the pagetable
  struct swap_s *s;
  struct buf *bp;
  if ((uint) (va) % PGSIZE)
    panic("swapread va");
#ifdef DEBUG_SWAPREAD
  cprintf("swapread: va: 0x%x pa: 0x%x\n", va, pa);
#endif
  for (int j = 0; j < (sb.nswap / SWBLOCKS); j++) {
    s = &swapll.buf[j];
//    cprintf("0x%x ", s->pte);
#ifdef DEBUG_SWAPREAD
    if ( s->taken )
      cprintf("swapread: pa: 0x%x va: 0x%x\n", s->pa, s->va);
#endif
    if (s->taken && s->pa == pa /*&& s->va == va*/) {
//      bwrite();
#ifdef DEBUG_SWAPREAD
      cprintf("swapread: FOUND!\n");
#endif
      void *pg = kalloc();
      acquiresleep(&s->lock);
      for (int i = 0; i < SWBLOCKS; ++i) {
        bp = bread(ROOTDEV, s->block + i);
        memmove(pg + i * BSIZE, bp->data, BSIZE);
        brelse(bp);
      }
      releasesleep(&s->lock);
      s->taken = FALSE;
      return pg;
    }
  }
  panic("swapread");
}
//#endif
//int is_swapped(pte_t* pte) {
//  struct swap_s *s;
//
////  acquire(&swapll.lock);
////  for (s = swapll.head.next; s != &swapll.head; s = s->next) {
//  for (int j = 0; j < (sb.nswap / (SWBLOCKS)); j++) {
//    s = &swapll.buf[j];
//    if (s->pte == pte){
//      return TRUE;
//    }
//  }
//  return FALSE;
//
//}
//TODO operation case for when all blocks are filled
//  for(uint snum = 0; snum < sb.nswap/4; snum++){
//    bp = bread(dev, IBLOCK(inum, sb));
//    dip = (struct dinode*)bp->data + inum%IPB;
//    if(dip->type == 0){  // a free inode
//      memset(dip, 0, sizeof(*dip));
//      dip->type = type;
//      log_write(bp);   // mark it allocated on the disk
//      brelse(bp);
//      return iget(dev, inum);
//    }
//    brelse(bp);
//  }
//  if(swap_file == 0)
//    panic("swapinit: null");
////  iunlock(swap_file);
//
////  cprintf("dev:%s\n", ip->dev);
//  end_op();
//};

