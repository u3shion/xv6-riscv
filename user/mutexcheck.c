#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"

static int fails = 0;

static void
check(int ok, const char *name)
{
  if(ok){
    printf("[OK] %s\n", name);
  } else {
    printf("[FAIL] %s\n", name);
    fails++;
  }
}

static void
test_rw_fstat_errors(void)
{
  int mfd;
  int r;
  char ch = 'x';
  struct stat st;

  mfd = mutex();
  check(mfd >= 0, "mutex create for read/write/fstat");
  if(mfd < 0)
    return;

  r = read(mfd, &ch, 1);
  check(r < 0, "read(mutex) returns error");

  r = write(mfd, &ch, 1);
  check(r < 0, "write(mutex) returns error");

  r = fstat(mfd, &st);
  check(r < 0, "fstat(mutex) returns error");

  check(mutex_close(mfd) == 0, "mutex_close after rw/fstat test");
}

static void
test_close_locked_by_owner(void)
{
  int mfd;

  mfd = mutex();
  check(mfd >= 0, "mutex create for close-by-owner");
  if(mfd < 0)
    return;

  check(mutex_lock(mfd) == 0, "mutex_lock by owner");
  check(mutex_close(mfd) == 0, "mutex_close on self-locked mutex");
  check(mutex_unlock(mfd) < 0, "mutex_unlock on closed fd fails");
}

static void
test_close_locked_by_other(void)
{
  int mfd, pid, st;

  mfd = mutex();
  check(mfd >= 0, "mutex create for close-by-other");
  if(mfd < 0)
    return;

  check(mutex_lock(mfd) == 0, "parent lock before child close");
  pid = fork();

  if(pid < 0){
    check(0, "fork for close-by-other");
    mutex_unlock(mfd);
    mutex_close(mfd);
    return;
  }

  if(pid > 0)
    check(1, "fork for close-by-other");

  if(pid == 0){
    int r = mutex_close(mfd);
    exit(r == 0 ? 0 : 1);
  }

  wait(&st);
  check(st == 0, "child closes its fd to locked mutex");
  check(mutex_unlock(mfd) == 0, "parent still owns lock");
  check(mutex_close(mfd) == 0, "parent closes last mutex fd");
}

static void
test_unlock_locked_by_other(void)
{
  int mfd, pid, st;

  mfd = mutex();
  check(mfd >= 0, "mutex create for unlock-by-other");
  if(mfd < 0)
    return;

  check(mutex_lock(mfd) == 0, "parent lock for unlock-by-other");
  pid = fork();

  if(pid < 0){
    check(0, "fork for unlock-by-other");
    mutex_unlock(mfd);
    mutex_close(mfd);
    return;
  }
  if(pid > 0)
    check(1, "fork for unlock-by-other");

  if(pid == 0){
    int r = mutex_unlock(mfd);
    mutex_close(mfd);
    exit(r < 0 ? 0 : 1);
  }

  wait(&st);
  check(st == 0, "child cannot unlock parent-owned mutex");
  check(mutex_unlock(mfd) == 0, "parent unlock succeeds");
  check(mutex_close(mfd) == 0, "parent close after unlock-by-other");
}

static void
test_exit_with_unclosed_locked_mutex(void)
{
  int mfd, pid, st;

  mfd = mutex();
  check(mfd >= 0, "mutex create for exit-with-unclosed");
  if(mfd < 0)
    return;

  pid = fork();

  if(pid < 0){
    check(0, "fork for exit-with-unclosed");
    mutex_close(mfd);
    return;
  }

  if(pid > 0)
    check(1, "fork for exit-with-unclosed");

  if(pid == 0){
    if(mutex_lock(mfd) < 0)
      exit(1);

    exit(0);
  }

  wait(&st);
  check(st == 0, "child exits while holding mutex");
  check(mutex_lock(mfd) == 0, "parent can lock after child exit");
  check(mutex_unlock(mfd) == 0, "parent unlock after child exit");
  check(mutex_close(mfd) == 0, "close after child exit cleanup");
}

int
main(void)
{
  test_rw_fstat_errors();
  test_close_locked_by_owner();
  test_close_locked_by_other();
  test_unlock_locked_by_other();
  test_exit_with_unclosed_locked_mutex();

  if(fails == 0){
    printf("ALL TESTS PASSED\n");
    exit(0);
  }

  printf("TESTS FAILED: %d\n", fails);
  exit(1);
}
