#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"

static void print_line_unsync(int argi, char c) {
  printf("pid: %d", getpid());
  pause(1);
  printf(", arg %d", argi);
  pause(1);
  printf(", char '%c'\n", c);
}

static void print_line_sync(int mfd, int argi, char c) {
  if (mutex_lock(mfd) < 0) {
    fprintf(2, "mutex_lock failed\n");
    exit(1);
  }

  printf("pid: %d", getpid());
  pause(1);
  printf(", arg %d", argi);
  pause(1);
  printf(", char '%c'\n", c);

  if (mutex_unlock(mfd) < 0) {
    fprintf(2, "mutex_unlock failed\n");
    exit(1);
  }
}

static void run_unsync(int argc, char **argv) {
  int i, j;

  for (i = 1; i < argc; i++) {
    for (j = 0; argv[i][j] != 0; j++) {
      print_line_unsync(i, argv[i][j]);
    }
  }
}

static void run_sync(int mfd, int argc, char **argv) {
  int i, j;

  for (i = 1; i < argc; i++) {
    for (j = 0; argv[i][j] != 0; j++) {
      print_line_sync(mfd, i, argv[i][j]);
    }
  }
}

int main(int argc, char **argv) {
  int pid;
  int st;
  int mfd;

  if (argc < 2) {
    fprintf(2, "usage: mutextest arg1 [arg2 ...]\n");
    exit(1);
  }

  printf("WITHOUT MUTEX:\n");

  pid = fork();

  if (pid < 0) {
    fprintf(2, "fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    run_unsync(argc, argv);
    exit(0);
  }
  
  run_unsync(argc, argv);
  wait(&st);

  printf("WITH MUTEX:\n");

  mfd = mutex();

  if (mfd < 0) {
    fprintf(2, "mutex create failed\n");
    exit(1);
  }

  pid = fork();
  if (pid < 0) {
    fprintf(2, "fork failed\n");
    mutex_close(mfd);
    exit(1);
  }

  if (pid == 0) {
    run_sync(mfd, argc, argv);
    mutex_close(mfd);
    exit(0);
  }

  run_sync(mfd, argc, argv);
  wait(&st);

  if (mutex_close(mfd) < 0) {
    fprintf(2, "mutex_close failed\n");
    exit(1);
  }

  exit(0);
}
