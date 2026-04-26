#include "log_server.h"
#include <stdarg.h>

volatile sig_atomic_t g_terminate_flag = 0;
volatile sig_atomic_t g_signal_received = 0;
volatile sig_atomic_t g_alarm_flag = 0;
volatile sig_atomic_t g_sigusr1_flag = 0;
volatile sig_atomic_t g_sighup_flag = 0;
volatile sig_atomic_t g_sigint_flag = 0;

int g_message_count = 0;
long g_total_bytes = 0;
int g_alarm_count = 0;

char g_fifo_path[256] = DEFAULT_FIFO_PATH;
char g_log_file_path[256] = DEFAULT_LOG_FILE;
FILE *g_log_fp = NULL;

void signal_handler(int signo)
{
    switch (signo)
    {
    case SIGTERM:
        g_terminate_flag = 1;
        g_signal_received = SIGTERM;
        g_sigint_flag = 0;
        break;

    case SIGINT:
        g_terminate_flag = 1;
        g_signal_received = SIGINT;
        g_sigint_flag = 1;
        break;

    case SIGQUIT:
        break;

    case SIGALRM:
        g_alarm_flag = 1;
        g_alarm_count++;
        break;

    case SIGUSR1:
        g_sigusr1_flag = 1;
        break;

    case SIGHUP:
        g_sighup_flag = 1;
        break;
    }
}

void setup_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;

    if (sigaction(SIGTERM, &sa, NULL) == -1)
    {
        perror("sigaction SIGTERM");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction SIGINT");
        exit(EXIT_FAILURE);
    }

    sa.sa_handler = SIG_IGN;

    if (sigaction(SIGQUIT, &sa, NULL) == -1)
    {
        perror("sigaction SIGQUIT");
        exit(EXIT_FAILURE);
    }

    sa.sa_handler = signal_handler;

    if (sigaction(SIGALRM, &sa, NULL) == -1)
    {
        perror("sigaction SIGALRM");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGUSR1, &sa, NULL) == -1)
    {
        perror("sigaction SIGUSR1");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGHUP, &sa, NULL) == -1)
    {
        perror("sigaction SIGHUP");
        exit(EXIT_FAILURE);
    }
}

void print_statistics(void)
{
    log_message("=== СТАТИСТИКА ===");
    log_message("Количество сообщений: %d", g_message_count);
    log_message("Общий объем данных: %ld байт", g_total_bytes);
    log_message("Количество срабатываний будильника: %d", g_alarm_count);
    log_message("==================");
}

void log_message(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    if (g_log_fp)
    {
        fprintf(g_log_fp, "[%s] ", timestamp);
        vfprintf(g_log_fp, format, args);
        fprintf(g_log_fp, "\n");
        fflush(g_log_fp);
    }

    va_end(args);
}

void cleanup(void)
{
    if (g_fifo_path[0] != '\0')
    {
        if (unlink(g_fifo_path) == 0)
            log_message("FIFO %s удален", g_fifo_path);
        else
            log_message("Не удалось удалить FIFO %s: %s", g_fifo_path, strerror(errno));
    }

    if (g_log_fp && g_log_fp != stdout && g_log_fp != stderr)
        fclose(g_log_fp);
}

void usage(const char *progname)
{
    fprintf(stderr, "Использование: %s [опции]\n", progname);
    fprintf(stderr, "Опции:\n");
    fprintf(stderr, "  -d          Запуск как демон\n");
    fprintf(stderr, "  -f PATH     Путь к FIFO (по умолчанию: %s)\n", DEFAULT_FIFO_PATH);
    fprintf(stderr, "  -l PATH     Путь к лог-файлу (по умолчанию: %s)\n", DEFAULT_LOG_FILE);
    fprintf(stderr, "  -a SEC      Интервал диагностики в секундах (по умолчанию: %d)\n", ALARM_INTERVAL);
    fprintf(stderr, "  -h          Показать эту справку\n");
}