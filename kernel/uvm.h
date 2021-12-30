#ifndef UVM_H_
#define UVM_H_

#include "pagetable.h"
#include "param.h"

// User memory is defined by Virtual Memory Areas,
// each indicating a range of directions
// with certain permissions and
// possibly backed by a file.
// The kernel performs lazy initilization and copy.
// Therefore, a vma page need not be mapped or mapped with write permissions,
// even when the vma has write permissions.
// A pagefault must be handled by the function uvmpagefault(...).
#define PROT_READ PTE_R
#define PROT_WRITE PTE_W
#define PROT_EXECUTE PTE_X
#define MAP_PRIVATE 0x00
#define MAP_SHARED 0x01

struct vma {
  int used;
  struct inode* inode;
  uint offset;  // only relevant with inode.
  uint filesz;  // only relevant with inode.
  uint64 start;
  uint64 length;
  uint perm;
  uint flags;
};

struct uvm {
  pagetable_t pagetable;      // User page table
  struct vma* vma[VMA_SIZE];  // Virtual Memory Areas
  struct vma* heap;           // Heap VMA (contained above)
};

// ─────────────────────────────────────────────────────────────────────────────
// User paging
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Initialize user virtual memory.
 */
void uvminit();

/**
 * Initialize the virtual memory for a user process given its trapframe.
 *
 * It adds the trampoline and trapframe pages to the pagetable,
 * but no other memory.
 */
int uvm_new(struct uvm* uvm, uint64 trapframe);

/**
 * Free the user memory, unmapping all the virtual memory areas.
 */
void uvm_free(struct uvm* uvm);

/**
 * Locate the process' vma associated to a virtual address.
 */
struct vma* uvm_va2vma(struct uvm* uvm, uint64 va);

/**
 * Create a new vma mapping.
 *
 * If the `struct inode* ip` is not null, then the vma is backed by a disk file.
 * If it is null, the vma is based in ram and the flags shall be `MAP_PRIVATE`.
 */
uint64 uvm_map(struct uvm* uvm, uint64 addr, uint64 length, uint perm,
               uint flags, struct inode* inode, uint offset, uint filesz);

/**
 * Unmap a range of address belonging to a virtual memory area.
 *
 * If the VMA is backed by a disk file and the flags is `MAP_SHARED`,
 * this function writes the modifications back to the file.
 * If the portion is the whole VMA, then the VMA is deleted.
 */
void uvm_unmap(struct uvm* uvm, uint64 addr, uint64 length);

/**
 * Handles a pagefault of the current process at address va.
 *
 * @returns the physical address of the new page.
 * @returns 0 if lack of memory or the page fault is due to wrong access.
 */
uint64 uvm_completemap(struct uvm* uvm, uint64 va, uint64 missing_perm);

/**
 * Makes sure a page is loaded into the user pagetable
 * so that copyin/out/instr do not require to sleep.
 *
 * @returns the physical address of the new page.
 * @returns 0 if lack of memory or the page fault is due to wrong access.
 */
uint64 uvm_guaranteecomplete(struct uvm* uvm, uint64 va, uint64 minimum_perm);

/**
 * Grow or shrink user memory by n bytes.
 *
 * @returns 0 on success
 * @returns -1 on failure.
 */
int uvm_growheap(struct uvm* uvm, int n);

/**
 * Duplicate the user virtual memory of the parent for the child,
 * performing the appropiate sharing of pages.
 */
int uvm_dup(struct uvm* parent, struct uvm* child);

// ─────────────────────────────────────────────────────────────────────────────
// Utilities
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Check if range does not intersect uvm's vmas.
 */
int uvm_israngefree(struct uvm* uvm, uint64 vastart, uint64 length);

/**
 * Select a virtual adress for a vma of length length.
 */
uint64 getfreevrange(struct uvm* uvm, int length);

/**
 * Create the user virtual memory for the first process.
 * (for the very first process).
 * sz must be less than a page.
 */
void code2uvm(struct uvm* uvm, uchar*, uint sz);

/**
 * Copy from kernel to user.
 * Copy len bytes from src to virtual address dstva in a given page table.
 *
 * @returns 0 on success.
 * @returns -1 on error.
 */
int copyout(struct uvm* uvm, uint64 dstva, char* src, uint64 len);

/**
 * Copy from user to kernel.
 * Copy len bytes to dst from virtual address srcva in a given page table.
 *
 * @returns 0 on success.
 * @returns -1 on error.
 */
int copyin(struct uvm* uvm, char* dst, uint64 srcva, uint64 len);

/**
 * Copy a null-terminated string from user to kernel.
 * Copy bytes to dst from virtual address srcva in a given page table,
 * until a '\0', or max.
 *
 * @returns 0 on success.
 * @returns -1 on error.
 */
int copyinstr(struct uvm* uvm, char*, uint64, uint64);

#endif
