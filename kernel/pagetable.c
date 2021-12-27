#include "defs.h"
#include "kalloc.h"
#include "memlayout.h"
#include "pagetable.h"
#include "param.h"

// ─────────────────────────────────────────────────────────────────────────────
// Pagetable primitives
// ─────────────────────────────────────────────────────────────────────────────

pagetable_t pgt_new() {
  pagetable_t pagetable = (pagetable_t)kalloc();
  if (pagetable == 0) {
    return 0;
  }
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

void pgt_free(pagetable_t pagetable) {
  // there are 2^9 = 512 PTEs in a page table.
  for (int i = 0; i < 512; i++) {
    pte_t pte = pagetable[i];
    if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0) {
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      pgt_free((pagetable_t)child);
      pagetable[i] = 0;
    } else if (pte & PTE_V) {
      panic("pgt_free: leaf");
    }
  }
  kfree((uint64)pagetable);
}

/**
 * Return the address of the PTE in pagetable.
 * If alloc!=0, create any required page-table pages.
 */
pte_t* pgt_walk(pagetable_t pagetable, uint64 va, int alloc) {
  if (va >= MAXVA) panic("walk");

  for (int level = 2; level > 0; level--) {
    pte_t* pte = &pagetable[PX(level, va)];
    if (*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if (!alloc || (pagetable = (pde_t*)kalloc()) == 0) return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

uint64 pgt_getpa(pagetable_t pagetable, uint64 va) {
  if (va >= MAXVA) return 0;
  pte_t* pte = pgt_walk(pagetable, va, 0);
  if (pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0) return 0;
  return PTE2PA(*pte);
}

int pgt_map(pagetable_t pagetable, uint64 va, uint64 pa, uint64 flags) {
  if (va >= MAXVA || va % PGSIZE != 0 || pa % PGSIZE != 0) {
    panic("pgt_map: Invalid addr\n");
  }
  pte_t* pte = pgt_walk(pagetable, va, 1);
  if (pte == 0) {
    return -1;
  }
  if (*pte & PTE_V) {
    panic("pgt_map: remap");
  }
  *pte = PA2PTE(pa) | flags | PTE_V | PTE_U;
  return 0;
}

void pgt_clearubit(pagetable_t pagetable, uint64 va) {
  pte_t* pte = pgt_walk(pagetable, va, 0);
  if (pte == 0) panic("pgt_clearubit");
  *pte &= ~PTE_U;
}

uint64 pgt_allocmap(pagetable_t pagetable, uint64 vastart, uint64 vaend,
                    uint64 flags) {
  if (vaend > MAXVA || vaend < vastart || vastart % PGSIZE != 0 ||
      vaend % PGSIZE != 0) {
    panic("pgt_allocmap: Invalid range\n");
  }

  for (uint64 addr = vastart; addr < vaend; addr += PGSIZE) {
    uint64 mem = kalloc();
    if (mem == 0) {
      pgt_deallocunmap(pagetable, vastart, addr);
      return 0;
    }
    memset((void*)mem, 0, PGSIZE);
    if (pgt_map(pagetable, addr, mem, flags) != 0) {
      kfree(mem);
      pgt_deallocunmap(pagetable, vastart, addr);
      return 0;
    }
  }
  return vaend;
}

void pgt_unmap_impl(pagetable_t pagetable, uint64 vastart, uint64 vaend,
                    int dealloc) {
  if (vaend > MAXVA || vaend < vastart || vastart % PGSIZE != 0 ||
      vaend % PGSIZE != 0) {
    printf("[%p %p)\n", vastart, vaend);
    panic("pgt_unmap_impl: Invalid range\n");
  }

  for (uint64 addr = vastart; addr < vaend; addr += PGSIZE) {
    pte_t* pte = pgt_walk(pagetable, addr, 0);
    if (pte == 0 || (*pte & PTE_V) == 0) continue;
    if (PTE_FLAGS(*pte) == PTE_V) panic("uvmunmap: not a leaf");
    if (dealloc) {
      kfree(PTE2PA(*pte));
    }
    *pte = 0;
  }
}

void pgt_unmap(pagetable_t pagetable, uint64 vastart, uint64 vaend) {
  pgt_unmap_impl(pagetable, vastart, vaend, 0);
}

void pgt_deallocunmap(pagetable_t pagetable, uint64 vastart, uint64 vaend) {
  pgt_unmap_impl(pagetable, vastart, vaend, 1);
}

int pgt_clone(pagetable_t src, pagetable_t dst, uint64 vastart, uint64 vaend) {
  if (vaend > MAXVA || vaend < vastart || vastart % PGSIZE != 0 ||
      vaend % PGSIZE != 0) {
    printf("[%p %p)\n", vastart, vaend);
    panic("pgt_clone: Invalid range\n");
  }
  uint64 addr;
  for (addr = vastart; addr < vaend; addr += PGSIZE) {
    pte_t* srcpte = pgt_walk(src, addr, 0);
    if (srcpte == 0 || (*srcpte & PTE_V) == 0) continue;
    uint64 pa = PTE2PA(*srcpte);
    pte_t* dstpte = pgt_walk(dst, addr, 1);
    if (dstpte == 0) goto err;

    // Remove write bit
    *srcpte &= ~((pte_t)PTE_W);
    *dstpte = *srcpte;
    kincref(pa);
  }
  return 0;

err:
  pgt_deallocunmap(dst, vastart, addr);
  return -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Kernel paging
// ─────────────────────────────────────────────────────────────────────────────

// kernel.ld sets this to end of kernel code.
extern char etext[];
// trampoline.S sets this to the beginning of the trampoline code.
extern char trampoline[];

// The kernel's page table.
pagetable_t kernel_pagetable;

void kvmmap(uint64 va, uint64 pa, uint64 sz, uint64 flags) {
  if (va % PGSIZE != 0 || pa % PGSIZE != 0 || sz % PGSIZE != 0 || sz <= 0) {
    panic("kvmmap: Invalid addr\n");
  }
  for (uint64 i = 0; i < sz; i += PGSIZE) {
    pte_t* pte = pgt_walk(kernel_pagetable, va, 1);
    if (pte == 0) {
      panic("kvmmap: walk\n");
    }
    if (*pte & PTE_V) {
      panic("kvmmap: remap");
    }
    *pte = PA2PTE(pa) | flags | PTE_V;
    va += PGSIZE;
    pa += PGSIZE;
  }
}

void kvminit(void) {
  kernel_pagetable = pgt_new();
  if (kernel_pagetable == 0) panic("kernel pagetable = 0\n");

  // uart registers
  kvmmap(UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // PLIC
  kvmmap(PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap((uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // Allocate a page for each process's kernel stack.
  // Map it high in memory, followed by an invalid
  // guard page.
  for (int i = 0; i < NPROC; i++) {
    uint64 pa = (uint64)kalloc();
    if (pa == 0) {
      panic("kvminit: kalloc");
    }
    uint64 va = KSTACK(i);
    kvmmap(va, pa, PGSIZE, PTE_R | PTE_W);
  }
}

void kvminithart() {
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}
