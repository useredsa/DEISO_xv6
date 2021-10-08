#include "kernel/types.h"
#include "kernel/pstat.h"
#include "user/user.h"
#include "kernel/stat.h"

void spin(int mul) {
  volatile unsigned x = 0;
  volatile unsigned y = 0;

  while (x < 100000) {
    y = 0;
    while (y < mul*1000) {
      y++;
    }
    x++;
  }
}

void printpinfo(int* pid, int npids) {
  struct pstat pi;
  int i;
  getpinfo(&pi);
  printf("%s\t%s\t\t%s\n", "PID", "TICKETS", "TICKS");
  for (i = 0; i < NPROC; i++) {
    for (int j = 0; j < npids; ++j) {
      if (pi.pid[i] == pid[j]) {
        printf("%d\t%d\t\t%d\n", pi.pid[i], pi.tickets[i], pi.ticks[i]);
      }
    }
  }
}

const int npids = 3;

int main(int argc, char *argv[]) {
  int pid[npids];
  int tickets[3] = {10, 20, 30};
  int work[3] = {48, 24, 16};
  settickets(100);

  for (int i = 0; i < npids; ++i) {
    if ((pid[i] = fork()) == 0) {
      int pid = getpid();
      printf("Process started with PID %d\n\n", pid);
      settickets(tickets[i]);
      spin(work[i]);
      printf("Process with PID %d finished!\n\n", pid);
      exit(0);
    }
  }

  int slices = 8;
  do {
    printpinfo(pid, npids);
    sleep(8);
  } while (slices--);

  for (int i = 0; i < npids; ++i) {
    wait(0);
  }
  exit(0);
}
