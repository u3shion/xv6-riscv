#include "kernel/types.h"
#include "user/user.h"

#define BUFF_SIZE 512

static int write_all(int fd, const char *buf, int len) {
  int written = 0;
  while (written < len) {
    int n = write(fd, buf + written, len - written);

    if (n <= 0)
      return -1;

    written += n;
  }
  return 0;
}

int main(int argc, char *argv[]) {
  int pipefd[2];
  int pid;
  char buf[BUFF_SIZE];

  if (pipe(pipefd) < 0) {
    fprintf(2, "pipe error\n");
    exit(1);
  }

  pid = fork();
  if (pid < 0) {
    fprintf(2, "fork error\n");
    exit(1);
  }

  if (pid == 0) {
    if (close(pipefd[1]) < 0) {
      fprintf(2, "close error\n");
      exit(1);
    }

    close(0);
    if (dup(pipefd[0]) < 0) {
      fprintf(2, "dup error\n");
      exit(1);
    }
    if (close(pipefd[0]) < 0) {
      fprintf(2, "close error\n");
      exit(1);
    }

    char *wc_argv[] = {"wc", 0};
    exec("/wc", wc_argv);

    fprintf(2, "exec wc error\n");
    exit(1);
  } else {
    if (close(pipefd[0]) < 0) {
      fprintf(2, "close error\n");
      exit(1);
    }

    int ofs = 0;

    for (int i = 1; i < argc; i++) {
      char *arg = argv[i];
      int len = strlen(arg);

      if (ofs + len + 1 > BUFF_SIZE) {
        if (write_all(pipefd[1], buf, ofs) < 0) {
          fprintf(2, "write error\n");
          exit(1);
        }

        ofs = 0;
      }

      memcpy(buf + ofs, arg, len);
      ofs += len;

      buf[ofs] = '\n';
      ofs++;
    }

    if (ofs > 0) {
      if (write_all(pipefd[1], buf, ofs) < 0) {
        fprintf(2, "write error\n");
        exit(1);
      }
    }

    if (close(pipefd[1]) < 0) {
      fprintf(2, "close error\n");
      exit(1);
    }

    wait(0);
    exit(0);
  }
}