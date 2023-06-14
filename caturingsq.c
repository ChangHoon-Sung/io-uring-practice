#define _GNU_SOURCE

#include <fcntl.h>
#include <liburing.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define QUEUE_DEPTH 8
#define BLOCK_SZ 4096

// Output a string of characters of len length to stdout.
void output_to_console(char *buf, int len) {
    while (len--) {
        fputc(*buf++, stdout);
    }
}

int get_completion(struct io_uring *ring) {
    struct io_uring_cqe *cqe;

    int ret = io_uring_wait_cqe(ring, &cqe);
    if (ret < 0) {
        fprintf(stderr, "io_uring_wait_cqe: %s\n", strerror(-ret));
        return ret;
    }
    io_uring_cqe_seen(ring, cqe);
    return 0;
}

int main(int argc, char *argv[]) {
    struct io_uring ring;
    struct io_uring_params params;
    int i, fd, ret;
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;
    struct stat st;
    ssize_t fsize;
    ssize_t bytes_remaining;
    off_t blocks;
    off_t offset;
    void *buf;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    memset(&params, 0, sizeof(params));
    params.flags |= IORING_SETUP_SQPOLL;
    params.sq_thread_idle = 2000;

    ret = io_uring_queue_init_params(QUEUE_DEPTH, &ring, &params);
    if (ret < 0) {
        fprintf(stderr, "io_uring_queue_init: %s\n", strerror(-ret));
        return 1;
    }

    fd = open(argv[1], O_RDONLY | O_DIRECT);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    ret = io_uring_register_files(&ring, &fd, 1);
    if (ret) {
        fprintf(stderr, "error registering files: %s", strerror(-ret));
        return 1;
    }

    if (fstat(fd, &st) < 0) {
        perror("fstat");
        return 1;
    }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "not a regular file\n");
        return 1;
    }
    if (st.st_size % BLOCK_SZ != 0) {
        fprintf(stderr, "file size is not a multiple of block size\n");
        return 1;
    }

    fsize = st.st_size;
    blocks = st.st_size / BLOCK_SZ;

    if (posix_memalign(&buf, BLOCK_SZ, BLOCK_SZ * blocks)) {
        perror("posix_memalign");
        return 1;
    }

    bytes_remaining = fsize;
    offset = 0;

    // read 4kb with zigzag pattern
    for (i = 0; i < blocks; i++) {
        // get a submission queue entry
        while ((sqe = io_uring_get_sqe(&ring)) == NULL) {
            // consume all completions
            while (io_uring_cq_ready(&ring) > 0) {
                ret = get_completion(&ring);
                if (ret < 0) {
                    fprintf(stderr, "io_uring_cqe_wait: %s\n", strerror(-ret));
                    return -1;
                }
                bytes_remaining -= BLOCK_SZ;
            }
            io_uring_submit(&ring);
        }

        // zigzag read
        if (i % 2 == 0) {
            io_uring_prep_read(sqe, 0, buf + i / 2 * BLOCK_SZ, BLOCK_SZ, offset);
            // fprintf(stderr, "read file from %ld to %ld ", offset, offset + BLOCK_SZ - 1);
            // fprintf(stderr, "to buf from %d to %d\n", i / 2 * BLOCK_SZ, i / 2 * BLOCK_SZ + BLOCK_SZ - 1);
        } else {
            io_uring_prep_read(sqe, 0, buf + fsize - BLOCK_SZ - i / 2 * BLOCK_SZ, BLOCK_SZ, fsize - offset - BLOCK_SZ);
            // fprintf(stderr, "read file from %ld to %ld ", fsize - offset - BLOCK_SZ, fsize - offset - BLOCK_SZ + BLOCK_SZ - 1);
            // fprintf(stderr, "to buf from %ld to %ld\n", fsize - BLOCK_SZ - i / 2 * BLOCK_SZ, fsize - BLOCK_SZ - i / 2 * BLOCK_SZ + BLOCK_SZ - 1);
        }
        io_uring_sqe_set_flags(sqe, IOSQE_FIXED_FILE);
        if (i > 0 && i % 2 == 1)
            offset += BLOCK_SZ;

        // sequential read
        // io_uring_prep_read(sqe, fd, buf + i * BLOCK_SZ, BLOCK_SZ, offset);
        // io_uring_sqe_set_flags(sqe, IOSQE_FIXED_FILE);
        // offset += BLOCK_SZ;

        if (io_uring_sq_space_left(&ring) == 0) {
            // submit all requests
            ret = io_uring_submit(&ring);
            if (ret < 0) {
                fprintf(stderr, "io_uring_submit: %s\n", strerror(-ret));
                return -1;
            } else if (ret != QUEUE_DEPTH) {
                fprintf(stderr, "io_uring_submit submitted less %d\n", ret);
                return -1;
            }
        }
    }

    io_uring_submit(&ring);
    if (ret < 0) {
        fprintf(stderr, "io_uring_submit: %s\n", strerror(-ret));
        return -1;
    }

    // wait for all requests to complete
    while (bytes_remaining > 0) {
        ret = get_completion(&ring);
        if (ret < 0) {
            fprintf(stderr, "io_uring_cqe_wait: %s\n", strerror(-ret));
            return -1;
        }
        bytes_remaining -= BLOCK_SZ;
    }

    // print the buffer to stdout
    // output_to_console(buf, fsize);

    free(buf);
    close(fd);
    io_uring_queue_exit(&ring);

    return 0;
}
