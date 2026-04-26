#ifndef LOG_SERVER_H
#define LOG_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define DEFAULT_FIFO_PATH "/tmp/log_server_fifo"
#define DEFAULT_LOG_FILE "/tmp/log_server.log"
#define BUFFER_SIZE 4096
#define ALARM_INTERVAL 10

extern volatile sig_atomic_t g_terminate_flag;
extern volatile sig_atomic_t g_signal_received;
extern volatile sig_atomic_t g_alarm_flag;
extern volatile sig_atomic_t g_sigusr1_flag;
extern volatile sig_atomic_t g_sighup_flag;
extern volatile sig_atomic_t g_sigint_flag;

extern int g_message_count;
extern long g_total_bytes;
extern int g_alarm_count;

void setup_signal_handlers(void);
void signal_handler(int signo);
void print_statistics(void);
void daemonize(void);
void cleanup(void);
void log_message(const char *format, ...);
void usage(const char *progname);

extern char g_fifo_path[256];
extern char g_log_file_path[256];
extern FILE *g_log_fp;

#endif // LOG_SERVER_H