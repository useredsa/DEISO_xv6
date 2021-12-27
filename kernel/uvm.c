#include "defs.h"
#include "file.h"
#include "kalloc.h"
#include "memlayout.h"
#include "pagetable.h"
#include "spinlock.h"
#include "uvm.h"

#define MIN(x, y) (y < x ? y : x)
#define MAX(x, y) (y > x ? y : x)

extern char trampoline[];  // trampoline.S

// ─────────────────────────────────────────────────────────────────────────────
// Vma primitives
// ─────────────────────────────────────────────────────────────────────────────

struct vma vmas[MAX_VMAS];
struct spinlock lock_vmas;

void uvminit() {
  initlock(&lock_vmas, "lock_vmas");
}

struct vma* vmaalloc(struct uvm* uvm) {
  int i;
  for (i = 0; i < VMA_SIZE; i++) {
    if (uvm->vma[i] == 0) break;
  }
  if (i == VMA_SIZE) return 0;
  int j;
  acquire(&lock_vmas);
  for (j = 0; j < MAX_VMAS; j++) {
    if (vmas[j].used == 0) {
      vmas[j].used = 1;
      release(&lock_vmas);
      uvm->vma[i] = &vmas[j];
      return uvm->vma[i];
    }
  }
  release(&lock_vmas);
  return 0;
}

void vma_init(struct vma* vma, uint64 start, uint64 length, uint perm,
              uint flags, struct inode* inode, uint offset, uint filesz) {
  vma->start = start;
  vma->length = length;
  vma->perm = perm;
  vma->flags = flags;
  if (inode != 0) {
    vma->inode = idup(inode);
  } else {
    vma->inode = inode;
  }
  vma->offset = offset;
  vma->filesz = filesz;
  
}

void vmafree(struct vma* vma) {
  if (vma->inode) {
    begin_op();
    iput(vma->inode);
    end_op();
  }
  acquire(&lock_vmas);
  vma->used = 0;
  release(&lock_vmas);
}

struct vma* vmadup(struct vma* vma) {
  int i;
  acquire(&lock_vmas);
  for (i = 0; i < MAX_VMAS; i++) {
    if (vmas[i].used == 0){
      vmas[i].used = 1;
      release(&lock_vmas);
      vmas[i].start = vma->start;
      vmas[i].length = vma->length;
      vmas[i].perm = vma->perm;
      vmas[i].flags = vma->flags;
      if (vma->inode) vmas[i].inode = idup(vma->inode);
      else vmas[i].inode = 0;
      vmas[i].offset = vma->offset;
      vmas[i].filesz = vma->filesz;
      return &vmas[i];
    }
  }
  release(&lock_vmas);
  return 0;
}

int vma_intersect(struct vma* v, struct vma* w) {
  uint64 l = MAX(PGROUNDDOWN(v->start), PGROUNDDOWN(w->start));
  uint64 r =
      MIN(PGROUNDUP(v->start + v->length), PGROUNDUP(w->start + w->length));
  return l < r;
}

// ─────────────────────────────────────────────────────────────────────────────
// User paging
// ─────────────────────────────────────────────────────────────────────────────

int uvm_new(struct uvm* uvm, uint64 trapframe) {
  memset(uvm, 0, sizeof(struct uvm));
  if ((uvm->pagetable = pgt_new()) == 0) return -1;

  // Map the trampoline code (for system call return)
  // at the highest user virtual address.
  if (pgt_map(uvm->pagetable, TRAMPOLINE, (uint64)trampoline, PTE_R | PTE_X)) {
    pgt_free(uvm->pagetable);
    uvm->pagetable = 0;
    return -1;
  }
  // Only the supervisor uses it, on the way to/from user space, so not PTE_U.
  pgt_clearubit(uvm->pagetable, (uint64)TRAMPOLINE);

  // Map the trapframe just below TRAMPOLINE, for trampoline.S.
  if (pgt_map(uvm->pagetable, TRAPFRAME, trapframe, PTE_R | PTE_W)) {
    pgt_deallocunmap(uvm->pagetable, TRAMPOLINE, TRAMPOLINE + PGSIZE);
    pgt_free(uvm->pagetable);
    uvm->pagetable = 0;
    return -1;
  }
  // Only the supervisor uses it, on the way to/from user space, so not PTE_U.
  pgt_clearubit(uvm->pagetable, (uint64)TRAPFRAME);

  return 0;
}

void uvm_free(struct uvm* uvm) {
  for (int i = 0; i < VMA_SIZE; ++i) {
    if (uvm->vma[i]) {
      uvm_unmap(uvm, uvm->vma[i]->start, uvm->vma[i]->length);
    }
  }
  if (uvm->pagetable == 0) panic("uvm_free");
  pgt_unmap(uvm->pagetable, TRAMPOLINE, TRAMPOLINE + PGSIZE);
  pgt_unmap(uvm->pagetable, TRAPFRAME, TRAPFRAME + PGSIZE);
  pgt_free(uvm->pagetable);
  uvm->pagetable = 0;
}

struct vma* uvm_va2vma(struct uvm* uvm, uint64 va) {
  for (struct vma** vma = uvm->vma; vma < uvm->vma + VMA_SIZE; ++vma) {
    if (*vma && (*vma)->start <= va && va < (*vma)->start + (*vma)->length) {
      return *vma;
    }
  }
  return 0;
}

int uvm_israngefree(struct uvm* uvm, uint64 vastart, uint64 length) {
  for (int i = 0; i < VMA_SIZE; ++i) {
    if (uvm->vma[i]) {
      uint64 l = MAX(PGROUNDDOWN(vastart), PGROUNDDOWN(uvm->vma[i]->start));
      uint64 r = MIN(PGROUNDUP(vastart + length),
                     PGROUNDUP(uvm->vma[i]->start + uvm->vma[i]->length));
      if (l < r) return 0;
    }
  }
  return 1;
}

uint64 getfreevrange(struct uvm* uvm, int length) {
  uint64 addr = START_VMAS_ADDR;
  while (1) {
    if (uvm_israngefree(uvm, addr, length)) return addr;
    // Otherwise, the next address to try shall be
    // the first end of vma higher than vma.
    uint64 previous_addr = addr;
    addr = MAXVA;
    for (int i = 0; i < VMA_SIZE; ++i) {
      if (uvm->vma[i]) {
        uint64 endvma = PGROUNDUP(uvm->vma[i]->start + uvm->vma[i]->length);
        if (endvma > previous_addr) {
          addr = MIN(addr, endvma);
        }
      }
    }
    addr = PGROUNDUP(addr);
    // If there is none, return an error.
    if (addr + length > MAXVA) break;
  }
  return 0;
}

uint64 uvm_map(struct uvm* uvm, uint64 addr, uint64 length, uint perm,
               uint flags, struct inode* inode, uint offset, uint filesz) {
  if (inode == 0 && flags != MAP_PRIVATE) return -1;

  struct vma* vma;
  if (!uvm_israngefree(uvm, addr, length)) return -1;
  if ((vma = vmaalloc(uvm)) == 0) return -1;
  vma_init(vma, addr, length, perm, flags, inode, offset, filesz);
  return addr;
}

void uvm_unmap(struct uvm* uvm, uint64 addr, uint64 length) {
  struct vma* vma = uvm_va2vma(uvm, addr);
  if (vma == 0) panic("uvm_unmap: not in vma!\n");
  if (addr != vma->start && addr + length != vma->start + vma->length)
    panic("uvm_unmap: not a valid mode\n");
  if (vma->flags == MAP_SHARED) {
    begin_op();
    ilock(vma->inode);
    for (uint64 va = PGROUNDDOWN(addr); va < PGROUNDUP(addr + length);
         va += PGSIZE) {
      uint64 pa = pgt_getpa(uvm->pagetable, va);
      if (pa == 0) continue;
      uint64 va0 = MAX(va, addr);
      uint64 va1 = MIN(va + PGSIZE, addr + length);
      va1 = MIN(va1, vma->start + vma->filesz);
      if (va1 <= va0) continue;
      uint64 pa0 = pa + (va0 - va);
      uint64 len = va1 - va0;
      int w = writei(vma->inode, 0, pa0, vma->offset + (va0 - vma->start), len);
      if (w != len) {
        panic("uvm_unmap: write error\n");
      }
    }
    iunlock(vma->inode);
    end_op();
  }
  if (vma->length == length) {
    pgt_deallocunmap(uvm->pagetable, PGROUNDDOWN(addr),
                     PGROUNDUP(addr + length));
    // If range is whole vma, free it.
    for (int i = 0; i < VMA_SIZE; i++) {
      if (vma == uvm->vma[i]) {
        uvm->vma[i] = 0;
        break;
      }
    }
    vmafree(vma);
  } else if (addr == vma->start) {
    pgt_deallocunmap(uvm->pagetable, PGROUNDDOWN(addr),
                     PGROUNDDOWN(addr + length));
    vma->start += length;
    vma->offset += length;
    vma->length -= length;
    vma->filesz = MAX(vma->filesz - length, 0);
  } else {
    pgt_deallocunmap(uvm->pagetable, PGROUNDUP(addr), PGROUNDUP(addr + length));
    vma->length -= length;
    vma->filesz = MIN(vma->filesz, vma->length);
  }
}

pte_t* pgt_walk(pagetable_t pagetable, uint64 va, int alloc);

uint64 uvm_completemap(struct uvm* uvm, uint64 va, uint64 missing_perm) {
  if (va % PGSIZE != 0 || va >= MAXVA) return 0;
  struct vma* vma = uvm_va2vma(uvm, va);
  if (!vma || (vma->perm & missing_perm) == 0) return 0;

  pte_t* pte = pgt_walk(uvm->pagetable, va, 1);
  if (pte == 0) return 0;

  if ((*pte & PTE_V) == 0) {
    // If pte did not exist, handle depending on whether its file.
    uint64 pa;
    if ((pa = kalloc()) == 0) return 0;
    *pte = PA2PTE(pa) | vma->perm | PTE_V | PTE_U;
    memset((char*)pa, 0, PGSIZE);
    if (vma->inode != 0) {
      // File -> read
      uint64 eof = vma->start + vma->filesz;
      if (va < eof) {
        uint64 readsz = MIN(eof - va, PGSIZE);
        ilock(vma->inode);
        int r =
            readi(vma->inode, 0, pa, vma->offset + (va - vma->start), readsz);
        iunlock(vma->inode);
        if (r != readsz) panic("uvm_completemap: readi");
      }
    }
    return pa;
  }

  // If it is valid but it is not a user page return error
  if ((*pte & PTE_U) == 0) return 0;

  if (missing_perm == PTE_W) {
    // If pte did exist and it was a write failure,
    // give write permissions if the physical page
    // is only referenced once,
    // or copy the page to a new page.
    uint64 pa = PTE2PA(*pte);
    if (ksingleref(pa)) {
      *pte |= PTE_W;
      return pa;
    }
    uint64 mem;
    if ((mem = kalloc()) == 0) {
      return 0;
    }
    memmove((void*)mem, (void*)pa, PGSIZE);
    *pte = PA2PTE(mem) | PTE_FLAGS(*pte) | PTE_W;
    kfree(pa);
    return mem;
  }
  printf("complete map part 2 %d %d\n", PTE_FLAGS(*pte) & 31,
         PTE_FLAGS(*pte) & missing_perm);
  // If pte did exist and it was other types of failures
  // that should not happen -> panic.
  printf("vaddr=%p paddr=%p\n", va, PTE2PA(*pte));
  return PTE2PA(*pte);
}

int uvm_growheap(struct uvm* uvm, int n) {
  struct vma* heap = uvm->heap;
  if (n > 0) {
    uint64 end = heap->start + heap->length;
    if (end > end + n || end + n > TRAPFRAME) return -1;
    heap->length += n;
    for (int i = 0; i < VMA_SIZE; ++i) {
      if (!uvm->vma[i] || uvm->vma[i] == heap) continue;
      if (vma_intersect(uvm->heap, uvm->vma[i])) {
        uvm->heap->length -= n;
        return -1;
      }
    }
  } else if (n < 0) {
    // Heap takes an extra non used page: see exec.
    // The reason is to avoid having empty vma.
    if (heap->length - PGSIZE < -n) return -1;
    uvm_unmap(uvm, heap->start + heap->length + n, -n);
  }
  return 0;
}

int uvm_dup(struct uvm* p, struct uvm* c) {
  for (int i = 0; i < VMA_SIZE; i++) {
    if (p->vma[i]) {
      c->vma[i] = vmadup(p->vma[i]);
      if (c->vma[i] == 0) goto err;
      if (pgt_clone(p->pagetable, c->pagetable, PGROUNDDOWN(p->vma[i]->start),
                    PGROUNDUP(p->vma[i]->start + p->vma[i]->length)) < 0) {
        vmafree(c->vma[i]);
        acquire(&lock_vmas);            
        c->vma[i]->used = 0;
        release(&lock_vmas); 
        goto err;
      }
      if (p->vma[i] == p->heap) c->heap = c->vma[i];
    }
  }
  return 0;

err:
  for (int i = 0; i < VMA_SIZE; i++) {
    if (c->vma[i]) {
      uvm_unmap(c, c->vma[i]->start, c->vma[i]->start + c->vma[i]->length);
    }
  }
  return -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Utilities
// ─────────────────────────────────────────────────────────────────────────────

void code2uvm(struct uvm* uvm, uchar* src, uint sz) {
  if (sz >= PGSIZE) panic("code2uvm: more than a page");
  struct vma* vma = vmaalloc(uvm);
  vma_init(vma, 0, PGSIZE, PTE_R | PTE_W | PTE_X, MAP_PRIVATE, 0, 0, 0);
  uint64 mem = kalloc();
  memset((void*)mem, 0, PGSIZE);
  pgt_map(uvm->pagetable, 0, mem, PTE_R | PTE_W | PTE_X);
  memmove((void*)mem, src, sz);
}

int copyout(struct uvm* uvm, uint64 dstva, char* src, uint64 len) {
  if (dstva + len < dstva || dstva + len > MAXVA) return -1;
  while (len > 0) {
    uint64 va0 = PGROUNDDOWN(dstva);
    pte_t* pte = pgt_walk(uvm->pagetable, va0, 0);
    uint64 pa0;
    if (pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_W) == 0) {
      if ((pa0 = uvm_completemap(uvm, va0, PTE_W)) == 0) return -1;
    } else {
      pa0 = PTE2PA(*pte);
    }
    uint64 n = MIN(PGSIZE - (dstva - va0), len);
    memmove((void*)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

int copyin(struct uvm* uvm, char* dst, uint64 srcva, uint64 len) {
  uint64 n, va0, pa0;

  while (len > 0) {
    va0 = PGROUNDDOWN(srcva);
    pa0 = pgt_getpa(uvm->pagetable, va0);
    if (pa0 == 0) {
      if ((pa0 = uvm_completemap(uvm, va0, PTE_R)) == 0) return -1;
    }
    n = PGSIZE - (srcva - va0);
    if (n > len) n = len;
    memmove(dst, (void*)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

int copyinstr(struct uvm* uvm, char* dst, uint64 srcva, uint64 max) {
  uint64 n, va0, pa0;
  int got_null = 0;

  while (got_null == 0 && max > 0) {
    va0 = PGROUNDDOWN(srcva);
    pa0 = pgt_getpa(uvm->pagetable, va0);
    if (pa0 == 0) {
      if ((pa0 = uvm_completemap(uvm, va0, PTE_R)) == 0) return -1;
    }
    n = PGSIZE - (srcva - va0);
    if (n > max) n = max;

    char* p = (char*)(pa0 + (srcva - va0));
    while (n > 0) {
      if (*p == '\0') {
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if (got_null) {
    return 0;
  } else {
    return -1;
  }
}
