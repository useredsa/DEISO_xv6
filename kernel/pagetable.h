#ifndef PAGETABLE_H_
#define PAGETABLE_H_

#include "riscv.h"

/**
 * The risc-v Sv39 scheme has three levels of page-table pages.
 * A page-table page contains 512 64-bit PTEs.
 * A 64-bit virtual address is split into five fields:
 *   39..63 -- must be zero.
 *   30..38 -- 9 bits of level-2 index.
 *   21..29 -- 9 bits of level-1 index.
 *   12..20 -- 9 bits of level-0 index.
 *    0..11 -- 12 bits of byte offset within the page.
 */

// ─────────────────────────────────────────────────────────────────────────────
// Pagetable primitives
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Reserve an empty page table.
 *
 * @returns 0 if out of memory.
 */
pagetable_t pgt_new();

/**
 * Recursively free page-table.
 * All leaf mappings must already have been removed.
 */
void pgt_free(pagetable_t pagetable);

/**
 * Look up a virtual address and return its physical address or 0 if not mapped.
 *
 * A table without PTE_V or PTE_U bits is considered invalid.
 *
 * Can only be used to look up user pages.
 */
uint64 pgt_getpa(pagetable_t pagetable, uint64 va);

/**
 * Allocate PTEs for va and map it to pa.
 *
 * The virtual adress must not be mapped.
 * The caller transfers the ownership of the physical adress reference
 * (meaning the function does not increase references).
 * The PTE_V and PTE_U flags are always added.
 *
 * @returns 0 on success.
 * @returns -1 if it could not allocate PTEs.
 */
int pgt_map(pagetable_t pagetable, uint64 va, uint64 pa, uint64 flags);

/**
 * Deallocate pages in the range [vastart, vaend)
 * without freeing the corresponding physical memory.
 *
 * @param vastart First address of range (must be page aligned).
 * @param vaend One past the last address of range (must be page aligned).
 * @returns vastart.
 */
void pgt_unmap(pagetable_t pagetable, uint64 vastart, uint64 vaend);

/**
 * Mark a leaf PTE invalid for user access.
 *
 * (Used by exec for the user stack guard page.)
 */
void pgt_clearubit(pagetable_t pagetable, uint64 va);

/**
 * Allocate PTEs and physical memory for the range [vastart, vaend).
 *
 * On failure, frees all the allocated memory.
 * The PTE_V and PTE_U flags are always added.
 *
 * @param vastart First address of range (must be page aligned).
 * @param vaend One past the last address of range (must be page aligned).
 * @returns vaend.
 * @returns 0 if it could not reserve memory.
 * @returns 0 if any page in the range was already mapped.
 */
uint64 pgt_allocmap(pagetable_t pagetable, uint64 vastart, uint64 vaend,
                    uint64 flags);

/**
 * Deallocate pages in the range [vastart, vaend),
 * freeing the corresponding physical memory.
 *
 * @param vastart First address of range (must be page aligned).
 * @param vaend One past the last address of range (must be page aligned).
 * @returns vastart.
 */
void pgt_deallocunmap(pagetable_t pagetable, uint64 vastart, uint64 vaend);

/**
 * Clone a range of virtual addresses to another pagetable.
 *
 * After the share, both tables reference the same physical memory and
 * lack write permissions to the range.
 * (Frees any allocated pages on failure.)
 *
 * @param vastart First address of range (must be page aligned).
 * @param vaend One past the last address of range (must be page aligned).
 * @returns 0 on success,
 * @returns -1 on failure.
 */
int pgt_clone(pagetable_t src, pagetable_t dst, uint64 vastart, uint64 vaend);

// ─────────────────────────────────────────────────────────────────────────────
// Kernel paging
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Allocate and initialize the unique kernel_pagetable.
 */
void kvminit();

/**
 * Add a mapping to the kernel page table.
 * The PTE_V flag is always added.
 * Only used when booting.
 * Does not flush TLB or enable paging.
 */
void kvmmap(uint64 va, uint64 pa, uint64 sz, uint64 flags);

/**
 * Switch h/w page table register to the kernel's page table and enable paging.
 */
void kvminithart();

#endif
