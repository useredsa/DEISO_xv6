#ifndef MMAP_H_
#define MMAP_H_

#define PROT_READ 0x01
#define PROT_WRITE 0x02
#define MAP_PRIVATE 0x00
#define MAP_SHARED 0x01
#define MAP_FAILED -1

struct vma {
  struct inode* inode;
  uint64 startaddr;
  uint64 endaddr;
  uint length;
  int prot;
  int flags;
  int used;
  int offset;
};

uint64 mmap(uint64 addr, uint64 length, int prot, int flags, struct file* f,
         int offset);
int munmap(uint64 addr, uint64 length);

struct vma* vmadup(struct vma* vma);

#endif
