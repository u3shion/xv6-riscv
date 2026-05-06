#include "kernel/types.h"
#include "user/user.h"

#define LOG_SYSCALLS   0x01
#define LOG_INTERRUPTS 0x02
#define LOG_PROC       0x04
#define LOG_EXEC       0x08
#define LOG_ALL        0x0f

static void
usage(void)
{
  fprintf(2,
    "Usage: logctl <command> [options]\n"
    "Commands:\n"
    "  off                  Disable all logging\n"
    "  on [classes] [ticks] Enable logging for the given classes\n"
    "                       (default: all classes, indefinite)\n"
    "  status               Show current logging flags\n"
    "Classes (can be combined):\n"
    "  syscalls             System calls\n"
    "  interrupts           Hardware interrupts\n"
    "  proc                 Process creation/termination\n"
    "  exec                 Application exec events\n"
    "  all                  All of the above\n"
    "Examples:\n"
    "  logctl on all        Enable all logging indefinitely\n"
    "  logctl on proc exec 500\n"
    "                       Enable proc+exec logging for 500 ticks\n"
    "  logctl off           Disable all logging\n");
  exit(1);
}

static int
parse_class(const char *s)
{
  if (strcmp(s, "syscalls")   == 0) return LOG_SYSCALLS;
  if (strcmp(s, "interrupts") == 0) return LOG_INTERRUPTS;
  if (strcmp(s, "proc")       == 0) return LOG_PROC;
  if (strcmp(s, "exec")       == 0) return LOG_EXEC;
  if (strcmp(s, "all")        == 0) return LOG_ALL;
  return -1;
}

int
main(int argc, char *argv[])
{
  if (argc < 2)
    usage();

  if (strcmp(argv[1], "off") == 0) {
    if (logctl(0, 0) < 0) {
      fprintf(2, "logctl: syscall failed\n");
      exit(1);
    }

    printf("logging disabled\n");
    exit(0);
  }

  if (strcmp(argv[1], "status") == 0) {
    printf("Use 'dmesg' to view logged messages.\n");
    exit(0);
  }

  if (strcmp(argv[1], "on") == 0) {
    int flags    = 0;
    int duration = 0;
    int i;

    for (i = 2; i < argc; i++) {
      int n = atoi(argv[i]);

      if (n > 0) {
        duration = n;
      } else {
        int f = parse_class(argv[i]);

        if (f < 0) {
          fprintf(2, "logctl: unknown class '%s'\n", argv[i]);
          usage();
        }

        flags |= f;
      }
    }

    if (flags == 0)
      flags = LOG_ALL;

    if (logctl(flags, duration) < 0) {
      fprintf(2, "logctl: syscall failed\n");
      exit(1);
    }

    printf("logging enabled: flags=0x%x", flags);
    if (duration > 0)
      printf(" for %d ticks", duration);
    printf("\n");
    
    exit(0);
  }

  usage();
  return 0;
}
