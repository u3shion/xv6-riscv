#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFF_SIZE 4096

static int write_all(int fd, const char *buf, size_t len) {
  size_t written = 0;
  while (written < len) {
    ssize_t n = write(fd, buf + written, len - written);

    if (n <= 0)
      return -1;

    written += (size_t)n;
  }
  return 0;
}

int main(int argc, char *argv[]) {
  int pipefd[2];
  pid_t pid;
  char buffer[BUFF_SIZE];
  int n;

  if (pipe(pipefd) == -1) {
    perror("pipe");
    exit(1);
  }

  pid = fork();
  if (pid == -1) {
    perror("fork");
    exit(1);
  }

  if (pid == 0) {
    if (close(pipefd[1]) == -1) {
      perror("close error");
      exit(1);
    }

    while ((n = read(pipefd[0], buffer, BUFF_SIZE)) > 0) {
      if (write_all(1, buffer, (size_t)n) < 0) {
        perror("write to stdout");
        exit(1);
      }
    }

    if (n == -1) {
      perror("read from pipe");
      exit(1);
    }

    if (close(pipefd[0]) == -1) {
      perror("close error");
      exit(1);
    }
    exit(0);
  } else {
    if (close(pipefd[0]) == -1) {
      perror("close error");
      exit(1);
    }

    char buf[BUFF_SIZE];
    int ofs = 0;

    for (int i = 1; i < argc; i++) {
      char *arg = argv[i];
      int len = (int)strlen(arg);

      if (ofs + len + 1 > BUFF_SIZE) {
        if (write_all(pipefd[1], buf, (size_t)ofs) < 0) {
          perror("write to pipe");
          exit(1);
        }

        ofs = 0;
      }

      memcpy(buf + ofs, arg, (size_t)len);
      ofs += len;
      buf[ofs] = '\n';
      ofs += 1;
    }

    if (ofs > 0) {
      if (write_all(pipefd[1], buf, (size_t)ofs) < 0) {
        perror("write to pipe");
        exit(1);
      }
    }

    if (close(pipefd[1]) == -1) {
      perror("close error");
      exit(1);
    }

    wait(NULL);
    exit(0);
  }
}