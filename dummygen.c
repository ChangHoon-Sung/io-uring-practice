#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <n>\n", argv[0]);
        fprintf(stderr, "n: number of 4KB blocks to write\n");
        return 1;
    }

    char file_name[] = "dummy.txt";
    char str[] = "0123456789abcdef";

    int n = atoi(argv[1]);
    int amount = (1 << 12);

    // open file with file_name
    int fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    for (int i = 0; i < n; i++)
        for (int j = 0; j < amount; j++)
            // append str[i % 16] to file
            write(fd, str + (i % 16), 1);

    close(fd);
    return 0;
}