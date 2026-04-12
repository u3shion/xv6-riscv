#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"

static char digits[] = "0123456789ABCDEF";

static void
printbyte(int fd, unsigned char b, int *fst)
{
  if (*fst == 0)
    write(fd, " ", 1);
  *fst = 0;

  char out[2];
  out[0] = digits[(b >> 4) & 0xF];
  out[1] = digits[b & 0xF];
  write(fd, out, 2);
}

int main(int argc, char *argv[])
{
  int n, fd;
  char buf[64];

  if (argc != 3)
  {
    fprintf(2, "Usage: hexdump <count> <file>\n");
    exit(1);
  }

  n = atoi(argv[1]);
  if (n < 0)
  {
    fprintf(2, "Read error\n");
    exit(1);
  }

  fd = open(argv[2], O_RDONLY);
  if (fd < 0)
  {
    fprintf(2, "Read error\n");
    exit(1);
  }

  int off = 0;
  int fst = 1;

  while (off < n)
  {
    int need = n - off;

    if (need > (int)sizeof(buf))
      need = sizeof(buf);

    int r = read(fd, buf, need);

    if (r < 0)
    {
      fprintf(2, "Read error\n");
      close(fd);
      exit(1);
    }

    if (r == 0)
      break;

    for (int i = 0; i < r; i++)
      printbyte(1, (unsigned char)buf[i], &fst);

    off += r;
  }

  write(1, "\n", 1);
  close(fd);
  exit(0);
}
