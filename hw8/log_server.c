#include "log_server.h"
#include <stdarg.h>

int alarm_interval = ALARM_INTERVAL;
int is_daemon = 0;

int read_from_fifo(int fd, int terminate_after_read)
{
    char buffer[BUFFER_SIZE + 1];
    ssize_t bytes_read;
    int total_read = 0;
    int eof_received = 0;

    while (!eof_received)
    {
        bytes_read = read(fd, buffer, BUFFER_SIZE);

        if (bytes_read > 0)
        {
            buffer[bytes_read] = '\0';
            int has_newline = (buffer[bytes_read - 1] == '\n');

            if (g_log_fp)
            {
                fprintf(g_log_fp, "%s", buffer);

                if (!has_newline)
                    fprintf(g_log_fp, "\n");

                fflush(g_log_fp);
            }

            g_total_bytes += bytes_read;
            total_read += bytes_read;
        }
        else if (bytes_read == 0)
        {
            eof_received = 1;
            break;
        }
        else
        {
            if (errno == EINTR)
            {
                if (g_terminate_flag)
                {
                    if (g_signal_received == SIGTERM)
                    {
                        log_message("Получен SIGTERM, завершение работы (данные не дочитываются)");
                        return -1;
                    }
                    else if (g_signal_received == SIGINT)
                    {
                        if (g_sigint_flag)
                        {
                            log_message("Получен SIGINT, дочитывание данных до конца");
                            terminate_after_read = 1;
                            continue;
                        }
                    }
                }

                if (g_alarm_flag)
                {
                    log_message("ДИАГНОСТИКА: Сервер работает и ожидает данных");
                    g_alarm_flag = 0;
                    alarm(alarm_interval);
                    continue;
                }

                if (g_sigusr1_flag)
                {
                    print_statistics();
                    g_sigusr1_flag = 0;
                    continue;
                }

                continue;
            }
            else
            {
                log_message("Ошибка чтения из FIFO: %s", strerror(errno));
                return -1;
            }
        }
    }

    if (terminate_after_read && g_signal_received == SIGINT)
    {
        g_message_count++;
        log_message("Сообщение дочитано по SIGINT");
        return -1;
    }

    return total_read;
}

int main(int argc, char *argv[])
{
    int opt;
    int foreground = 1;

    while ((opt = getopt(argc, argv, "df:l:a:h")) != -1)
    {
        switch (opt)
        {
        case 'd':
            foreground = 0;
            is_daemon = 1;
            break;

        case 'f':
            strncpy(g_fifo_path, optarg, sizeof(g_fifo_path) - 1);
            g_fifo_path[sizeof(g_fifo_path) - 1] = '\0';
            break;

        case 'l':
            strncpy(g_log_file_path, optarg, sizeof(g_log_file_path) - 1);
            g_log_file_path[sizeof(g_log_file_path) - 1] = '\0';
            break;

        case 'a':
            alarm_interval = atoi(optarg);

            if (alarm_interval <= 0)
                alarm_interval = ALARM_INTERVAL;

            break;

        case 'h':
            usage(argv[0]);
            exit(EXIT_SUCCESS);

        default:
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    setup_signal_handlers();

    if (!foreground)
    {
        g_log_fp = fopen(g_log_file_path, "a");

        if (!g_log_fp)
        {
            perror("Не удалось открыть лог-файл");
            exit(EXIT_FAILURE);
        }

        daemonize();
    }
    else
        g_log_fp = stdout;

    log_message("Запуск лог-сервера");
    log_message("FIFO: %s", g_fifo_path);
    log_message("Лог-файл: %s", foreground ? "stdout" : g_log_file_path);
    log_message("Интервал диагностики: %d сек", alarm_interval);

    struct stat st;

    if (mkfifo(g_fifo_path, 0600) == -1)
    {
        if (errno == EEXIST)
        {
            if (stat(g_fifo_path, &st) == -1)
            {
                log_message("Ошибка stat для %s: %s", g_fifo_path, strerror(errno));
                exit(EXIT_FAILURE);
            }

            if (!S_ISFIFO(st.st_mode))
            {
                log_message("Файл %s существует, но не является FIFO", g_fifo_path);
                exit(EXIT_FAILURE);
            }

            log_message("Используем существующий FIFO: %s", g_fifo_path);
        }
        else
        {
            log_message("Ошибка создания FIFO: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    if (alarm(alarm_interval) == (unsigned int)-1)
        log_message("Ошибка установки будильника");

    while (!g_terminate_flag)
    {
        int fd;

        if (g_sighup_flag && foreground)
        {
            g_sighup_flag = 0;

            log_message("Получен SIGHUP, выполняется демонизация");
            print_statistics();

            fclose(g_log_fp);
            g_log_fp = fopen(g_log_file_path, "a");

            if (!g_log_fp)
            {
                log_message("Ошибка открытия лог-файла после демонизации");
                exit(EXIT_FAILURE);
            }

            daemonize();
            foreground = 0;
            is_daemon = 1;

            log_message("Демонизация выполнена");
            continue;
        }

        fd = open(g_fifo_path, O_RDONLY);

        if (fd == -1)
        {
            if (errno == EINTR)
            {
                if (g_terminate_flag)
                {
                    log_message("Завершение работы по сигналу %s", g_signal_received == SIGINT ? "SIGINT" : "SIGTERM");
                    break;
                }

                if (g_alarm_flag)
                {
                    log_message("ДИАГНОСТИКА: Сервер работает и ожидает данных");
                    g_alarm_flag = 0;
                    alarm(alarm_interval);
                }

                if (g_sigusr1_flag)
                {
                    print_statistics();
                    g_sigusr1_flag = 0;
                }

                continue;
            }
            else
            {
                log_message("Ошибка открытия FIFO: %s", strerror(errno));
                break;
            }
        }

        log_message("FIFO открыт для чтения, ожидание данных...");

        int read_result = read_from_fifo(fd, 0);
        g_message_count++;

        close(fd);

        if (read_result == -1 && g_terminate_flag)
            break;

        if (g_sigusr1_flag)
        {
            print_statistics();
            g_sigusr1_flag = 0;
        }
    }

    log_message("Завершение работы лог-сервера");
    print_statistics();
    cleanup();

    return EXIT_SUCCESS;
}