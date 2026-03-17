#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define LOG_FILE "/tmp/daemon.log"

volatile sig_atomic_t running = 1;

void handle_signal(int sig) {
    running = 0;
}

void write_log(const char *msg) {
    FILE *f = fopen(LOG_FILE, "a");
    if (f) {
        time_t now = time(NULL);
        fprintf(f, "[%s] %s\n", ctime(&now), msg);
        fclose(f);
    }
}

int main() {
    // Создаём дочерний процесс
    if (fork() != 0) exit(0);
    
    // Создаём новую сессию
    setsid();
    
    // Обработчик сигнала завершения
    signal(SIGTERM, handle_signal);
    
    // Записываем PID
    FILE *pid = fopen("/tmp/daemon.pid", "w");
    if (pid) {
        fprintf(pid, "%d\n", getpid());
        fclose(pid);
    }
    
    write_log("Демон запущен");
    
    // Основной цикл
    while (running) {
        write_log("Работаю...");
        sleep(10);
    }
    
    write_log("Демон остановлен");
    unlink("/tmp/daemon.pid");
    
    return 0;
}
