#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int err = settickets(5);
  if (err != 0) {
    printf("Unsuccessful syscall... %d\n", err);
  } else {
    printf("Successful syscall!\n");
  }
  exit(0);
}
