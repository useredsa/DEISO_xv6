#include "defs.h"
#include "file.h"
#include "memlayout.h"
#include "mmap.h"
#include "param.h"
#include "proc.h"
#include "types.h"

struct vma vmas[MAX_VMAS];

#define MIN(x, y) (y < x ? y : x)
#define MAX(x, y) (y > x ? y : x)

uint64 addrblock(struct proc* p, int length) {
  int i;
  uint64 l, r;
  int intersects = 1;
  uint64 addr = 0;
  uint64 nextaddr;
  while (intersects) {
    if (addr == 0) {
      nextaddr = START_VMAS_ADDR;
    } else {
      nextaddr = MAXVA - length;
      for (i = 0; i < VMA_SIZE; ++i) {
        if (p->vma[i]) {
          if (p->vma[i]->endaddr >= addr) {
            nextaddr = MIN(nextaddr, p->vma[i]->endaddr);
          }
        }
      }
    }
    if (nextaddr == addr) return 0;
    addr = nextaddr;
    intersects = 0;
    for (i = 0; i < VMA_SIZE; ++i) {
      if (p->vma[i]) {
        l = MAX(addr, p->vma[i]->startaddr);
        r = MIN(addr + length, p->vma[i]->endaddr);
        if (l < r) {
          intersects = 1;
          break;
        }
      }
    }
  }
  return addr;
}

struct vma* vmaalloc(struct proc* p, uint64 addr, uint64 length, int prot,
                     int flags, struct inode* ip, int offset) {
  int i;
  for (i = 0; i < VMA_SIZE; i++) {
    if (p->vma[i] == 0) break;
  }
  if (i == VMA_SIZE) return 0;
  int j;
  for (j = 0; j < MAX_VMAS; j++) {
    if (vmas[j].used == 0) {
      vmas[j].used = 1;
      vmas[j].startaddr = addr;
      vmas[j].endaddr = addr + length;
      vmas[j].prot = prot;
      vmas[j].flags = flags;
      vmas[j].inode = idup(ip);
      vmas[j].offset = offset;
      p->vma[i] = &vmas[j];
      return &vmas[j];
    }
  }
  return 0;
}

uint64 mmap(uint64 addr, uint64 length, int prot, int flags, struct file* f,
            int offset) {
  struct vma* vma;
  struct proc* p = myproc();
  int r;
  uint64 pte_prot = PTE_U;
  if (f->type != FD_INODE && f->type != FD_DEVICE) return MAP_FAILED;
  if ((addr = addrblock(p, length)) == 0) return MAP_FAILED;
  if (prot & PROT_READ) {
    if (!f->readable) return MAP_FAILED;
    pte_prot |= PTE_R;
  }
  if (prot & PROT_WRITE) {
    if (flags != MAP_PRIVATE && !f->writable) return MAP_FAILED;
    pte_prot |= PTE_W;
  }
  if (uvmalloc(p->pagetable, addr, addr + length, pte_prot) == 0)
    return MAP_FAILED;
  if ((vma = vmaalloc(p, addr, length, prot, flags, f->ip, offset)) == 0)
    return MAP_FAILED;
  ilock(f->ip);
  r = readi(f->ip, 1, addr, offset, length);
  // f->ip->size = MAX(f->ip->size, addr+length);
  iunlock(f->ip);
  if (r == -1) {
    uvmdealloc(p->pagetable, addr + length, addr);
    for (int i = 0; i < VMA_SIZE; ++i) {
      if (p->vma[i] == vma) {
        p->vma[i] = 0;
        vma[i].used = 0;
      }
    }
    return MAP_FAILED;
  }
  return addr;
}

struct vma* getvma(struct proc* p, uint64 addr) {
  for (int i = 0; i < VMA_SIZE; i++) {
    if (p->vma[i] && p->vma[i]->startaddr <= addr && addr < p->vma[i]->endaddr)
      return p->vma[i];
  }
  return 0;
}

int munmap(uint64 addr, uint64 length) {
  struct proc* p = myproc();
  struct vma* vma = getvma(p, addr);
  if (addr != vma->startaddr && addr + length != vma->endaddr) return -1;
  if (vma->flags == MAP_SHARED) {
    struct file f;
    f.type = FD_INODE;
    f.ref = 1;
    f.writable = 1;
    f.ip = vma->inode;
    f.off = vma->offset;
    filewrite(&f, addr, length);
  }
  uvmdealloc(p->pagetable, addr + length, addr);
  if (addr == vma->startaddr) {
    if (addr + length == vma->endaddr) {
      vma->used = 0;
      for (int i = 0; i < VMA_SIZE; i++) {
        if (vma == p->vma[i]) {
          p->vma[i] = 0;
          break;
        }
      }
    } else {
      vma->startaddr += length;
    }
  } else {
    vma->endaddr -= length;
  }
  return 0;
}

struct vma* vmadup(struct vma* vma) {
  int i;
  for(i=0; i<MAX_VMAS; i++){
    if(vmas[i].used == 0) break;
  }
  if(i==MAX_VMAS) return 0;
  vmas[i].used = vma->used;
  vmas[i].startaddr = vma->startaddr;
  vmas[i].endaddr = vma->endaddr;
  vmas[i]. prot =  vma->prot;
  vmas[i].flags =  vma->flags;
  vmas[i].inode = idup(vma->inode);
  vmas[i].offset = vma->offset;
  return &vmas[i];
}
