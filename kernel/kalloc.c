/**
 * Physical memory allocator, for user processes, kernel stacks,
 * page-table pages, and pipe buffers.
 * Allocates whole 4096-byte pages.
 */
#include "defs.h"
#include "kalloc.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "types.h"

#define MAXPAGES (PHYSTOP / PGSIZE)

void freerange(uint64 pa_start, uint64 pa_end);

extern char end[];  // first address after kernel.
                    // defined by kernel.ld.

struct {
  struct spinlock lock;
  uint64 numfree;
  int refs[MAXPAGES];
  uint64 freelist[MAXPAGES];
} kmem;

void kinit() {
  initlock(&kmem.lock, "kmem");
  freerange((uint64)end, (uint64)PHYSTOP);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a call to kalloc().
// (The exception is when initializing the allocator; see kinit above.)
void krelease(uint64 pa) {
  // Fill with junk to catch dangling refs.
  memset((void*)pa, 1, PGSIZE);
  kmem.freelist[kmem.numfree] = pa >> PGSHIFT;
  kmem.numfree++;
}

void freerange(uint64 pa_start, uint64 pa_end) {
  for (uint64 p = PGROUNDUP(pa_start); p + PGSIZE <= pa_end; p += PGSIZE) {
    krelease(p);
  }
}

uint64 kalloc(void) {
  uint64 index;
  uint64 pa;

  acquire(&kmem.lock);
  if (kmem.numfree == 0) {
    release(&kmem.lock);
    return 0;
  }
  kmem.numfree--;
  index = kmem.freelist[kmem.numfree];
  release(&kmem.lock);
  kmem.refs[index] = 1;
  pa = index << PGSHIFT;
  memset((void*)pa, 5, PGSIZE);  // fill with junk
  return pa;
}

void kincref(uint64 pa) {
  if ((pa % PGSIZE) != 0 || (char*)pa < end || pa >= PHYSTOP) panic("kincref");
  acquire(&kmem.lock);
  uint64 index = pa >> PGSHIFT;
  kmem.refs[index]++;
  release(&kmem.lock);
}

void kfree(uint64 pa) {
  if ((pa % PGSIZE) != 0 || (char*)pa < end || pa >= PHYSTOP) panic("kfree");
  acquire(&kmem.lock);
  uint64 index = pa >> PGSHIFT;
  kmem.refs[index]--;
  if (kmem.refs[index] < 0) {
    panic("kfree: refs below 0\n");
  }
  if (kmem.refs[index] == 0) {
    krelease(pa);
  }
  release(&kmem.lock);
}

int ksingleref(uint64 pa) {
  if ((pa % PGSIZE) != 0 || (char*)pa < end || pa >= PHYSTOP)
    panic("ksingleref");
  acquire(&kmem.lock);
  uint64 index = (uint64)pa >> PGSHIFT;
  int single = kmem.refs[index] == 1;
  release(&kmem.lock);
  return single;
}
