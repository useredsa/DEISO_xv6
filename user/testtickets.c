#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if (settickets(5) != 0) {
    printf("Unsuccessful syscall!\n");
  } else {
    printf("Successful syscall!\n");
  }
  if (settickets(0) != -1) {
    printf("Unsuccessful syscall!\n");
  } else {
    printf("Successful syscall!\n");
  }
  if (settickets(-1) != -1) {
    printf("Unsuccessful syscall!\n");
  } else {
    printf("Successful syscall!\n");
  }
  exit(0);
}
