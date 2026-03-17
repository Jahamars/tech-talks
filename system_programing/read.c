#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#define BUFFER_SIZE 1024

int main() {
    const char *filename = "data.txt";
    char buffer[BUFFER_SIZE];
    
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Ошибка открытия");
        return 1;
    }
    
    ssize_t bytes = read(fd, buffer, BUFFER_SIZE - 1);
    if (bytes == -1) {
        perror("Ошибка чтения");
        close(fd);
        return 1;
    }
    
    buffer[bytes] = '\0';
    printf("Прочитано: %ld байт\n", bytes);
    printf("Содержимое:\n%s", buffer);
    close(fd);
    
    return 0;
}
