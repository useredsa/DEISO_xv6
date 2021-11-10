// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "kalloc.h"

#define MAXPAGES (PHYSTOP / PGSIZE)

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct {
  struct spinlock lock;
  uint64 numfree;
  int refs[MAXPAGES];
  uint64 freelist[MAXPAGES];
} kmem;



void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  // printf("kfree %p %d\n", pa, kmem.numfree);
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  kmem.freelist[kmem.numfree] = (uint64) pa >> PGSHIFT;
  kmem.numfree++;
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

void *
kalloc(void)
{
  uint64 index;
  void* pa;

  acquire(&kmem.lock);
  if (kmem.numfree == 0) {
    release(&kmem.lock);
    return 0;
  }
  kmem.numfree--;
  index = kmem.freelist[kmem.numfree];
  release(&kmem.lock);
  kmem.refs[index] = 1;
  pa = (void*) (index << PGSHIFT);
  memset((char*)pa, 5, PGSIZE); // fill with junk
  return pa;
}

void 
kincref(void *pa){
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kincref");
  acquire(&kmem.lock);
  int index = (uint64) pa >> PGSHIFT;
  kmem.refs[index]++;
  release(&kmem.lock);
}

void 
kdecref(void *pa){
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kdecref");
  acquire(&kmem.lock);
  int index = (uint64) pa >> PGSHIFT;
  kmem.refs[index]--;
  if(kmem.refs[index] == 0){
    kfree(pa);
  }
  release(&kmem.lock);
}
