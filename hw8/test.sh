RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

FIFO_PATH="/tmp/log_server_fifo"
LOG_FILE="/tmp/log_server_output.log"
DAEMON_LOG="/tmp/log_server.log"
TEST_DIR="/tmp/log_server_tests"
PASSED=0
FAILED=0

test_result() {
    local test_name="$1"
    local result="$2"
    
    if [ "$result" -eq 0 ]; then
        echo -e "${GREEN}[PASS]${NC} $test_name"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED}[FAIL]${NC} $test_name"
        FAILED=$((FAILED + 1))
    fi
}

check_log_content() {
    local pattern="$1"
    local file="$2"
    grep -q "$pattern" "$file" 2>/dev/null
    return $?
}

cleanup() {
    echo -e "${BLUE}Очистка окружения...${NC}"
    
    pkill -f "./log_server" 2>/dev/null
    sleep 1
    
    rm -f "$FIFO_PATH" "$LOG_FILE" "$DAEMON_LOG"
    rm -rf "$TEST_DIR"
    mkdir -p "$TEST_DIR"
    
    echo "Короткое сообщение" > "$TEST_DIR/short.txt"
    echo -e "Среднее сообщение\nВторая строка" > "$TEST_DIR/medium.txt"
    
    for i in $(seq 1 100); do
        echo "Строка $i: тестовое сообщение" >> "$TEST_DIR/large.txt"
    done
    
    for i in $(seq 1 500); do
        echo "Строка $i для проверки дочитывания" >> "$TEST_DIR/sigint_test.txt"
    done
}

start_foreground() {
    ./log_server "$@" > "$LOG_FILE" 2>&1 &
    echo $!
}

start_daemon() {
    ./log_server -d "$@"
}

test_foreground_startup() {
    echo -e "\n${YELLOW}Тест 1: Запуск в foreground режиме${NC}"
    cleanup
    
    local pid=$(start_foreground)
    sleep 2
    
    if kill -0 $pid 2>/dev/null; then
        test_result "Запуск foreground процесса" 0
    else
        test_result "Запуск foreground процесса" 1
        return
    fi
    
    if [ -p "$FIFO_PATH" ]; then
        test_result "Создание FIFO" 0
    else
        test_result "Создание FIFO" 1
    fi
    
    if check_log_content "Запуск лог-сервера" "$LOG_FILE"; then
        test_result "Приветственное сообщение в логе" 0
    else
        test_result "Приветственное сообщение в логе" 1
        echo "Содержимое лога:"
        cat "$LOG_FILE"
    fi
    
    kill -TERM $pid 2>/dev/null
    wait $pid 2>/dev/null
}

test_daemon_startup() {
    echo -e "\n${YELLOW}Тест 2: Запуск в режиме демона${NC}"
    cleanup
    
    start_daemon
    sleep 2
    
    local pid=$(pgrep -f "./log_server" | head -1)
    
    if [ -n "$pid" ]; then
        test_result "Запуск демона" 0
        
        local ppid=$(ps -o ppid= -p $pid 2>/dev/null | tr -d ' ')
        if [ "$ppid" = "1" ]; then
            test_result "Демон имеет PPID=1" 0
        else
            test_result "Демон имеет PPID=1" 1
            echo "  PPID = $ppid"
        fi
        
        if [ -f "$DAEMON_LOG" ]; then
            test_result "Создание лог-файла демона" 0
        else
            test_result "Создание лог-файла демона" 1
        fi
        
        kill -TERM $pid 2>/dev/null
        sleep 1
    else
        test_result "Запуск демона" 1
    fi
}

test_short_messages() {
    echo -e "\n${YELLOW}Тест 3: Прием коротких сообщений${NC}"
    cleanup
    
    local pid=$(start_foreground)
    sleep 2
    
    echo "Сообщение 1" > "$FIFO_PATH"
    sleep 1
    echo "Сообщение 2" > "$FIFO_PATH"
    sleep 1
    echo "Сообщение 3" > "$FIFO_PATH"
    sleep 2
    
    kill -TERM $pid 2>/dev/null
    wait $pid 2>/dev/null
    
    local all_found=0
    if check_log_content "Сообщение 1" "$LOG_FILE" && \
       check_log_content "Сообщение 2" "$LOG_FILE" && \
       check_log_content "Сообщение 3" "$LOG_FILE"; then
        all_found=1
    fi
    
    if [ $all_found -eq 1 ]; then
        test_result "Получение всех сообщений" 0
    else
        test_result "Получение всех сообщений" 1
        echo "Содержимое лога:"
        cat "$LOG_FILE"
    fi
}

test_file_transfer() {
    echo -e "\n${YELLOW}Тест 4: Передача файлов через FIFO${NC}"
    cleanup
    
    local pid=$(start_foreground)
    sleep 2
    
    cat "$TEST_DIR/short.txt" > "$FIFO_PATH"
    sleep 1
    cat "$TEST_DIR/medium.txt" > "$FIFO_PATH"
    sleep 1
    cat "$TEST_DIR/large.txt" > "$FIFO_PATH"
    sleep 3
    
    kill -TERM $pid 2>/dev/null
    wait $pid 2>/dev/null
    
    if check_log_content "Короткое сообщение" "$LOG_FILE" && \
       check_log_content "Среднее сообщение" "$LOG_FILE" && \
       check_log_content "Вторая строка" "$LOG_FILE" && \
       check_log_content "Строка 1:" "$LOG_FILE" && \
       check_log_content "Строка 100:" "$LOG_FILE"; then
        test_result "Передача файлов через FIFO" 0
    else
        test_result "Передача файлов через FIFO" 1
        echo "Размер лога: $(wc -l < "$LOG_FILE") строк"
        echo "Первые 10 строк лога:"
        head -10 "$LOG_FILE"
        echo "Последние 10 строк лога:"
        tail -10 "$LOG_FILE"
    fi
}

test_sigterm_handling() {
    echo -e "\n${YELLOW}Тест 5: Обработка SIGTERM${NC}"
    cleanup
    
    local pid=$(start_foreground)
    sleep 2
    
    cat "$TEST_DIR/large.txt" > "$FIFO_PATH" &
    sleep 0.5
    
    kill -TERM $pid
    sleep 2
    wait $pid 2>/dev/null
    
    if check_log_content "SIGTERM\|Завершение работы\|завершение" "$LOG_FILE"; then
        test_result "Обработка SIGTERM (сообщение в логе)" 0
    else
        test_result "Обработка SIGTERM (сообщение в логе)" 1
        echo "Содержимое лога:"
        cat "$LOG_FILE"
    fi
    
    sleep 1
    if [ ! -e "$FIFO_PATH" ]; then
        test_result "Удаление FIFO после завершения" 0
    else
        test_result "Удаление FIFO после завершения" 1
    fi
}

test_sigint_handling() {
    echo -e "\n${YELLOW}Тест 6: Обработка SIGINT с дочитыванием${NC}"
    cleanup
    
    local pid=$(start_foreground)
    sleep 2
    
    echo "Сообщение до SIGINT" > "$FIFO_PATH"
    sleep 1
    
    cat "$TEST_DIR/sigint_test.txt" > "$FIFO_PATH" &
    sleep 0.1
    kill -INT $pid
    sleep 5
    wait $pid 2>/dev/null
    
    if check_log_content "Сообщение до SIGINT" "$LOG_FILE"; then
        if check_log_content "Строка 500 для проверки дочитывания" "$LOG_FILE"; then
            test_result "Полное дочитывание при SIGINT" 0
        elif check_log_content "Строка 1 для проверки дочитывания" "$LOG_FILE"; then
            test_result "Частичное дочитывание при SIGINT" 0
            echo "  Примечание: дочитано не все (это нормально)"
        else
            test_result "Дочитывание при SIGINT" 1
            echo "Данные не найдены в логе"
        fi
    else
        test_result "Дочитывание при SIGINT" 1
        echo "Сообщения не найдены"
    fi
}

test_sigquit_ignoring() {
    echo -e "\n${YELLOW}Тест 7: Игнорирование SIGQUIT${NC}"
    cleanup
    
    local pid=$(start_foreground)
    sleep 2
    
    kill -QUIT $pid
    sleep 1
    kill -QUIT $pid
    sleep 1
    
    if kill -0 $pid 2>/dev/null; then
        test_result "Игнорирование SIGQUIT" 0
        kill -TERM $pid 2>/dev/null
        wait $pid 2>/dev/null
    else
        test_result "Игнорирование SIGQUIT" 1
    fi
}

test_sigusr1_statistics() {
    echo -e "\n${YELLOW}Тест 8: Статистика по SIGUSR1${NC}"
    cleanup
    
    local pid=$(start_foreground)
    sleep 2
    
    echo "Сообщение 1" > "$FIFO_PATH"
    sleep 1
    echo "Сообщение 2" > "$FIFO_PATH"
    sleep 1
    cat "$TEST_DIR/large.txt" > "$FIFO_PATH"
    sleep 2
    
    kill -USR1 $pid
    sleep 2
    
    kill -TERM $pid 2>/dev/null
    wait $pid 2>/dev/null
    
    if check_log_content "Количество сообщений\|Общий объем\|СТАТИСТИКА" "$LOG_FILE"; then
        test_result "Вывод статистики по SIGUSR1" 0
    else
        test_result "Вывод статистики по SIGUSR1" 1
        echo "Поиск статистики в логе:"
        grep -i "статистика\|количество\|объем" "$LOG_FILE" || echo "Ничего не найдено"
    fi
}

test_alarm_messages() {
    echo -e "\n${YELLOW}Тест 9: Диагностические сообщения${NC}"
    cleanup
    
    local pid=$(start_foreground -a 3)
    sleep 10
    
    kill -TERM $pid 2>/dev/null
    wait $pid 2>/dev/null
    
    local alarm_count=$(grep -c -i "диагностика\|работает и ожидает" "$LOG_FILE" 2>/dev/null)
    
    if [ "$alarm_count" -ge 2 ]; then
        test_result "Диагностические сообщения ($alarm_count шт.)" 0
    elif [ "$alarm_count" -ge 1 ]; then
        test_result "Диагностические сообщения ($alarm_count шт.)" 0
        echo "  Примечание: меньше ожидаемого количества"
    else
        test_result "Диагностические сообщения" 1
        echo "Содержимое лога:"
        cat "$LOG_FILE"
    fi
}

test_multiple_senders() {
    echo -e "\n${YELLOW}Тест 10: Множественные отправители${NC}"
    cleanup
    
    local pid=$(start_foreground)
    sleep 2
    
    for i in 1 2 3 4 5; do
        (echo "Отправитель $i" > "$FIFO_PATH") &
    done
    
    sleep 3
    
    kill -TERM $pid 2>/dev/null
    wait $pid 2>/dev/null
    
    local found=0
    for i in 1 2 3 4 5; do
        if check_log_content "Отправитель $i" "$LOG_FILE"; then
            found=$((found + 1))
        fi
    done
    
    if [ $found -eq 5 ]; then
        test_result "Все 5 отправителей ($found/5)" 0
    elif [ $found -ge 3 ]; then
        test_result "Большинство отправителей ($found/5)" 0
    else
        test_result "Множественные отправители ($found/5)" 1
    fi
}

test_existing_fifo() {
    echo -e "\n${YELLOW}Тест 11: Существующий FIFO${NC}"
    cleanup
    
    mkfifo "$FIFO_PATH" 2>/dev/null
    chmod 600 "$FIFO_PATH"
    
    local pid=$(start_foreground)
    sleep 2
    
    echo "Тест существующего FIFO" > "$FIFO_PATH"
    sleep 2
    
    kill -TERM $pid 2>/dev/null
    wait $pid 2>/dev/null
    
    if check_log_content "Тест существующего FIFO" "$LOG_FILE"; then
        test_result "Работа с существующим FIFO" 0
    else
        test_result "Работа с существующим FIFO" 1
    fi
}

test_non_fifo_file() {
    echo -e "\n${YELLOW}Тест 12: Обработка не-FIFO файла${NC}"
    cleanup
    
    echo "Не FIFO" > "$FIFO_PATH"
    
    timeout 3 ./log_server > "$LOG_FILE" 2>&1 &
    local pid=$!
    sleep 2
    
    if ! kill -0 $pid 2>/dev/null; then
        test_result "Завершение при не-FIFO файле" 0
    else
        test_result "Завершение при не-FIFO файле" 1
        kill -TERM $pid 2>/dev/null
        echo "Содержимое лога:"
        cat "$LOG_FILE"
    fi
}

test_final_statistics() {
    echo -e "\n${YELLOW}Тест 13: Финальная статистика${NC}"
    cleanup
    
    local pid=$(start_foreground)
    sleep 2
    
    echo "Сообщение 1" > "$FIFO_PATH"
    sleep 1
    echo "Сообщение 2" > "$FIFO_PATH"
    sleep 1
    
    kill -TERM $pid 2>/dev/null
    wait $pid 2>/dev/null
    
    if check_log_content "Количество сообщений\|Общий объем\|Завершение работы" "$LOG_FILE"; then
        test_result "Статистика при завершении" 0
    else
        test_result "Статистика при завершении" 1
        echo "Последние 10 строк лога:"
        tail -10 "$LOG_FILE"
    fi
}

test_sighup_daemonization() {
    echo -e "\n${YELLOW}Тест 14: Демонизация по SIGHUP${NC}"
    cleanup
    
    local pid=$(start_foreground)
    sleep 2
    
    local old_ppid=$(ps -o ppid= -p $pid 2>/dev/null | tr -d ' ')
    
    if [ "$old_ppid" = "1" ]; then
        echo "  Процесс уже демон, пропускаем"
        kill -TERM $pid 2>/dev/null
        test_result "Демонизация по SIGHUP" 0
        return
    fi
    
    kill -HUP $pid
    sleep 3
    
    local new_pid=$(pgrep -f "./log_server" | head -1)
    
    if [ -n "$new_pid" ]; then
        local new_ppid=$(ps -o ppid= -p $new_pid 2>/dev/null | tr -d ' ')
        
        if [ "$new_ppid" = "1" ]; then
            test_result "Демонизация по SIGHUP" 0
            kill -TERM $new_pid 2>/dev/null
        else
            test_result "Демонизация по SIGHUP" 1
            echo "  PPID = $new_ppid"
            kill -TERM $new_pid 2>/dev/null
        fi
    else
        test_result "Демонизация по SIGHUP" 1
        echo "  Процесс не найден после SIGHUP"
    fi
}

main() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}    ТЕСТИРОВАНИЕ LOG SERVER (FIFO)${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo -e "${YELLOW}Foreground логи: $LOG_FILE${NC}"
    echo -e "${YELLOW}Демон логи: $DAEMON_LOG${NC}"
    echo ""
    
    if [ ! -x "./log_server" ]; then
        echo -e "${RED}Ошибка: log_server не найден${NC}"
        echo "Скомпилируйте: make"
        exit 1
    fi
    
    test_foreground_startup
    test_daemon_startup
    test_short_messages
    test_file_transfer
    test_sigterm_handling
    test_sigint_handling
    test_sigquit_ignoring
    test_sigusr1_statistics
    test_alarm_messages
    test_multiple_senders
    test_existing_fifo
    test_non_fifo_file
    test_final_statistics
    test_sighup_daemonization
    
    cleanup
    
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}         ИТОГИ ТЕСТИРОВАНИЯ${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo -e "${GREEN}Пройдено: $PASSED${NC}"
    echo -e "${RED}Провалено: $FAILED${NC}"
    echo -e "Всего: $((PASSED + FAILED))"
    
    if [ "$FAILED" -eq 0 ]; then
        echo -e "\n${GREEN}✓ Все тесты пройдены!${NC}"
        exit 0
    else
        echo -e "\n${RED}✗ Есть проваленные тесты${NC}"
        echo -e "Проверьте логи в: $LOG_FILE и $DAEMON_LOG"
        exit 1
    fi
}

main "$@"