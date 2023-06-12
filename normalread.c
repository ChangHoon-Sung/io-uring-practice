#define _GNU_SOURCE

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BLOCK_SZ 4096

int main(int argc, char *argv[]) {
    struct stat st;
    ssize_t fsize;
    int blocks;
    void *buf;
    int fd, ret;

    fd = open("../1G.bin", O_RDONLY | O_DIRECT);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    ret = fstat(fd, &st);
    if (ret < 0) {
        perror("fstat");
        return 1;
    }
    blocks = st.st_size / BLOCK_SZ;
    if (st.st_size % BLOCK_SZ != 0)
        blocks++;

    ret = posix_memalign(&buf, BLOCK_SZ, blocks * BLOCK_SZ);
    if (ret < 0) {
        perror("posix_memalign");
        return 1;
    }

    for (int i = 0; i < blocks; i++) {
        read(fd, buf + i, BLOCK_SZ);
    }

    close(fd);
    return 0;
}