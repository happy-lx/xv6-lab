// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  int ref_counter[TOTAL_PHYSICAL_PAGES];
} kmem;

// get the idx in ref_counter array given a physical address
int
get_idx(void *pa)
{
  return ((uint64)pa - (uint64)KERNBASE) / PGSIZE;
}

void 
increment_ref_counter(void *pa)
{
  int idx = get_idx(pa);
  acquire(&kmem.lock);
  kmem.ref_counter[idx]++;
  release(&kmem.lock);
}

void 
decrement_ref_counter(void *pa)
{
  int idx = get_idx(pa);
  kmem.ref_counter[idx]--;
}

// this function will aquire lock
int 
get_ref_counter(void* pa) {
  acquire(&kmem.lock);
  return kmem.ref_counter[get_idx(pa)];
}

void 
release_kmem_lock()
{
  release(&kmem.lock);
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  // set ref_counter to 1 so that when we fisrt free all pages, we can truely free them all.
  for(int i=0; i<TOTAL_PHYSICAL_PAGES; i++)
    kmem.ref_counter[i] = 1;
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  
  acquire(&kmem.lock);
  // decrease the ref_counter
  kmem.ref_counter[get_idx(pa)] -= 1;
  
  int counter = kmem.ref_counter[get_idx(pa)];
  if(counter < 0) {
    panic("kfree: counter < 0");
  }else if(counter == 0) {
    // truely free the page when counter == 0
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    r = (struct run*)pa;
    r->next = kmem.freelist;
    kmem.freelist = r;
  }else {
    // do nothing
  }
  release(&kmem.lock);
}

// for debug
int
free_pages()
{
  int res = 0;
  for(int i=0; i<TOTAL_PHYSICAL_PAGES; i++) {
    if(kmem.ref_counter[i] == 0)
      res++;
  }
  return res;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    kmem.ref_counter[get_idx((void*) r)] = 1;
    // printf("free_page numbers :%d\n", free_pages());
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// the version of kalloc without acquiring lock
void *
kalloc_without_lock(void)
{
  struct run *r;

  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    kmem.ref_counter[get_idx((void*) r)] = 1;
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
