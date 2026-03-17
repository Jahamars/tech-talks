#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int main() {
    const char *filename = "data.txt";
    const char *text = "cmon man let hem cook\n";
    
    int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Ошибка открытия");
        return 1;
    }
    
    ssize_t written = write(fd, text, strlen(text));
    if (written == -1) {
        perror("Ошибка записи");
        close(fd);
        return 1;
    }
    
    printf("Записано: %ld байт\n", written);
    close(fd);
    
    return 0;
}
