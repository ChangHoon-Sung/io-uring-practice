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
    int blocks, offset;
    void *buf;
    int fd, ret;

    fd = open(argv[1], O_RDONLY | O_DIRECT);
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

    offset = 0;
    for (int i = 0; i < blocks; i++) {
        if (i % 2 == 0) {
            // read file from file offset to buf
            lseek(fd, offset, SEEK_SET);
            read(fd, buf + i / 2 * BLOCK_SZ, BLOCK_SZ);
        } else {
            // read file from file offset to buf
            lseek(fd, fsize - offset - BLOCK_SZ, SEEK_SET);
            read(fd, buf + fsize - BLOCK_SZ - i / 2 * BLOCK_SZ, BLOCK_SZ);
        }
        if (i > 0 && i % 2 == 1)
            offset += BLOCK_SZ;
    }

    close(fd);
    return 0;
}