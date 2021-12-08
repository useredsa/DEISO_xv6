#include "defs.h"
#include "elf.h"
#include "memlayout.h"
#include "param.h"
#include "proc.h"
#include "riscv.h"
#include "spinlock.h"
#include "types.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

// // TODO
static int loadseg(struct uvm *uvm, uint64 addr, struct inode *ip, uint offset,
                   uint sz);

int exec(char *path, char **argv) {
  char *s, *last;
  int i, off;
  uint64 argc, highest_addr = 0, sp, ustack[MAXARG], stackbase;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  struct uvm uvm;
  struct proc *p = myproc();
  memset(&uvm, 0, sizeof(struct uvm));
  //TODO cmmts
  // printf("exec pid=%d %s\n", p->pid, path);

  begin_op();

  if ((ip = namei(path)) == 0) {
    end_op();
    return -1;
  }
  ilock(ip);

  // Check ELF header
  if (readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf)) goto bad;
  if (elf.magic != ELF_MAGIC) goto bad;

  if (uvm_new(&uvm, (uint64)p->trapframe)) goto bad;

  // Load program into memory.
  for (i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph)) {
    if (readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph)) goto bad;
    if (ph.type != ELF_PROG_LOAD) continue;
    if (ph.memsz < ph.filesz) goto bad;
    if (ph.vaddr + ph.memsz < ph.vaddr) goto bad;
    if ((ph.vaddr % PGSIZE) != 0) goto bad;
    uint perm = 0;
    perm |= ph.flags & ELF_PROG_FLAG_READ ? PTE_R : 0;
    perm |= ph.flags & ELF_PROG_FLAG_WRITE ? PTE_W : 0;
    perm |= ph.flags & ELF_PROG_FLAG_EXEC ? PTE_X : 0;
    if (uvm_map(&uvm, ph.vaddr, ph.memsz, perm,
                perm & PTE_W ? MAP_PRIVATE : MAP_SHARED, ip, ph.off,
                ph.filesz) == MAP_FAILED)
      goto bad;
    if (loadseg(&uvm, ph.vaddr, ip, ph.off, ph.filesz) < 0) goto bad;
    highest_addr = MAX(highest_addr, ph.vaddr + ph.memsz);
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  p = myproc();

  // Allocate two pages at the next page boundary.
  // Use the second as the user stack.
  highest_addr = PGROUNDUP(highest_addr);
  if (uvm_map(&uvm, highest_addr, 2 * PGSIZE, PTE_R | PTE_W | PTE_X,
              MAP_PRIVATE, 0, 0, 0) == MAP_FAILED)
    goto bad;
  uvm_completemap(&uvm, highest_addr, PTE_R);
  pgt_clearubit(uvm.pagetable, highest_addr);
  sp = highest_addr + 2 * PGSIZE;
  stackbase = sp - PGSIZE;

  // Create a vma representing the heap of size PGSIZE
  if (uvm_map(&uvm, sp, PGSIZE, PTE_R | PTE_W | PTE_X, MAP_PRIVATE, 0, 0, 0) ==
      MAP_FAILED)
    goto bad;
  uvm.heap = uvm_va2vma(&uvm, sp);

  // Push argument strings, prepare rest of stack in ustack.
  for (argc = 0; argv[argc]; argc++) {
    if (argc >= MAXARG) goto bad;
    sp -= strlen(argv[argc]) + 1;
    sp -= sp % 16;  // riscv sp must be 16-byte aligned
    if (sp < stackbase) goto bad;
    if (copyout(&uvm, sp, argv[argc], strlen(argv[argc]) + 1) < 0) goto bad;
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  // push the array of argv[] pointers.
  sp -= (argc + 1) * sizeof(uint64);
  sp -= sp % 16;
  if (sp < stackbase) goto bad;
  if (copyout(&uvm, sp, (char *)ustack, (argc + 1) * sizeof(uint64)) < 0)
    goto bad;

  // arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.
  p->trapframe->a1 = sp;

  // Save program name for debugging.
  for (last = s = path; *s; s++)
    if (*s == '/') last = s + 1;
  safestrcpy(p->name, last, sizeof(p->name));

  // Commit to the user image.
  uvm_free(&p->uvm);
  p->uvm = uvm;
  p->trapframe->epc = elf.entry;  // initial program counter = main
  p->trapframe->sp = sp;          // initial stack pointer

  return argc;  // this ends up in a0, the first argument to main(argc, argv)

bad:
  uvm_free(&uvm);
  if (ip) {
    iunlockput(ip);
    end_op();
  }
  return -1;
}

// Load a program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
static int loadseg(struct uvm *uvm, uint64 va, struct inode *ip, uint offset,
                   uint sz) {
  uint i, n;
  uint64 pa;

  for (i = 0; i < sz; i += PGSIZE) {
    pa = pgt_getpa(uvm->pagetable, va + i);
    // printf("loadseg: request completemap %d %d\n", offset, sz);
    if (pa == 0) {
      pa = uvm_completemap(uvm, va + i, PTE_R);
      if (pa == 0) panic("loadseg: address should exist");
    }
    if (sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    printf("LOADSEG %p %p %p\n", pa, offset+i, n);
    // if (readi(ip, 0, (uint64)pa, offset + i, n) != n) return -1;
  }

  return 0;
}
