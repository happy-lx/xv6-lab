// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define HASHSIZE 13
#define HASH(blockno) (blockno % HASHSIZE)

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf heads[HASHSIZE];
  struct spinlock hash_locks[HASHSIZE];
} bcache;

char lock_names[HASHSIZE][512];

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // init hash bucket 
  for(int i=0; i<HASHSIZE; i++) {
    bcache.heads[i].prev = &bcache.heads[i];
    bcache.heads[i].next = &bcache.heads[i];
    snprintf(lock_names[i], 512, "bcache_%d", i);
    initlock(&bcache.hash_locks[i], lock_names[i]);
  }
  int cnt = 0;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->valid = 0;
    b->blockno = cnt++;
    b->dev = 0xdeadbeef;
    b->timestamp = 0;
    int slot = HASH(b->blockno);
    b->next = bcache.heads[slot].next;
    b->prev = &bcache.heads[slot];
    initsleeplock(&b->lock, "buffer");
    bcache.heads[slot].next->prev = b;
    bcache.heads[slot].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int slot;
retry:
  slot = HASH(blockno);
  acquire(&bcache.hash_locks[slot]);

  // Is the block already cached?
  for(b = bcache.heads[slot].next; b != &bcache.heads[slot]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      b->timestamp = ticks;
      release(&bcache.hash_locks[slot]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  release(&bcache.hash_locks[slot]);
  acquire(&bcache.lock);

  struct buf* found;
research:
  found = 0;
  for(int i=0; i<NBUF; i++) {
    if(bcache.buf[i].refcnt == 0) {
      if(found == 0 || bcache.buf[i].timestamp < found->timestamp) {
        found = bcache.buf + i;
      }
    }
    if(bcache.buf[i].dev == dev && bcache.buf[i].blockno == blockno) {
      release(&bcache.lock);
      goto retry;
    }
  }

  if(!found) {
    panic("bget: no buffers");
  }

  int slot_old = HASH(found->blockno);
  acquire(&bcache.hash_locks[slot_old]);

  if(found->refcnt != 0) {
    release(&bcache.hash_locks[slot_old]);
    goto research;
  }

  found->next->prev = found->prev;
  found->prev->next = found->next;

  release(&bcache.hash_locks[slot_old]);

  found->dev = dev;
  found->blockno = blockno;
  found->valid = 0;
  found->refcnt = 1;
  found->timestamp = ticks;

  acquire(&bcache.hash_locks[slot]);

  found->next = bcache.heads[slot].next;
  found->prev = &bcache.heads[slot];
  bcache.heads[slot].next->prev = found;
  bcache.heads[slot].next = found;

  release(&bcache.hash_locks[slot]);

  release(&bcache.lock);
  acquiresleep(&found->lock);

  return found;
  
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  int slot = HASH(b->blockno);
  acquire(&bcache.hash_locks[slot]);
  b->refcnt--;
  release(&bcache.hash_locks[slot]);
}

void
bpin(struct buf *b) {
  int slot = HASH(b->blockno);
  acquire(&bcache.hash_locks[slot]);
  b->refcnt++;
  release(&bcache.hash_locks[slot]);
}

void
bunpin(struct buf *b) {
  int slot = HASH(b->blockno);
  acquire(&bcache.hash_locks[slot]);
  b->refcnt--;
  release(&bcache.hash_locks[slot]);
}

