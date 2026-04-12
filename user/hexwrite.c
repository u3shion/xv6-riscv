#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"

static int
hexval(int c)
{
  if (c >= '0' && c <= '9')
    return c - '0';

  if (c >= 'A' && c <= 'F')
    return 10 + c - 'A';

  if (c >= 'a' && c <= 'f')
    return 10 + c - 'a';

  return -1;
}

int main(int argc, char *argv[])
{
  int fd, n;
  char *hex, *buf;

  if (argc != 3)
  {
    fprintf(2, "Usage: hexwrite <hex> <file>\n");
    exit(1);
  }

  hex = argv[1];
  int len = strlen(hex);
  if ((len % 2) != 0)
  {
    fprintf(2, "Write error\n");
    exit(1);
  }

  n = len / 2;
  buf = malloc(n);
  if (buf == 0)
  {
    fprintf(2, "Write error\n");
    exit(1);
  }

  for (int i = 0; i < n; i++)
  {
    int h = hexval(hex[2 * i]);
    int l = hexval(hex[2 * i + 1]);

    if (h < 0 || l < 0)
    {
      fprintf(2, "Write error\n");
      free(buf);
      exit(1);
    }

    buf[i] = (h << 4) | l;
  }

  fd = open(argv[2], O_WRONLY);
  if (fd < 0)
  {
    fprintf(2, "Write error\n");
    free(buf);
    exit(1);
  }

  int written = 0;

  while (written < n)
  {
    int w = write(fd, buf + written, n - written);

    if (w <= 0)
    {
      fprintf(2, "Write error\n");
      close(fd);
      free(buf);
      exit(1);
    }

    written += w;
  }
  close(fd);
  free(buf);

  exit(0);
}
