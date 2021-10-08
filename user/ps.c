#include "kernel/pstat.h"
#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  struct pstat procstatus;
  getpinfo(&procstatus);

  printf("%s\t%s\t\t%s\n", "PID", "TICKETS", "TICKS");
  for (int i = 0; i < NPROC; i++) {
    if (procstatus.inuse[i]) {
      printf("%d\t%d\t\t%d\n", procstatus.pid[i], procstatus.tickets[i],
             procstatus.ticks[i]);
    }
  }
  exit(0);
}
