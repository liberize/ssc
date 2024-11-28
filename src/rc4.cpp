#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "rc4.h"

int main(int argc, const char **argv) {
    if (argc < 4) {
        return 1;
    }
    int n = argc >= 5 ? atoi(argv[4]) : 1;
    int offset = argc >= 6 ? atoi(argv[5]) : 0;
    int fd_in = open(argv[1], O_RDONLY);
    if (fd_in == -1) {
        LOGE("failed to open input file");
        return 1;
    }
    int fd_out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out == -1) {
        LOGE("failed to open output file");
        return 1;
    }
    int size = (int) lseek(fd_in, 0, SEEK_END);
    lseek(fd_in, offset, SEEK_SET);
    if (size == -1) {
        LOGE("failed to seek to end of file");
        return 1;
    }
    size -= offset;
    n = std::max(std::min(n, size), 1);
    int buf_size = (size + n - 1) / n;
    char *buf = (char*) malloc(buf_size);
    if (!buf) {
        LOGE("failed to alloc buffer");
        return 1;
    }
    while (size > 0) {
        int seg_size = std::min(size, buf_size);
        size -= seg_size;
        if (read(fd_in, buf, seg_size) != seg_size) {
            LOGE("failed to read file");
            return 1;
        }
        LOGD("encrypt segment. size=%d", seg_size);
        rc4((u8*) buf, seg_size, (u8*) argv[3], strlen(argv[3]));
        if (write(fd_out, buf, seg_size) != seg_size) {
            LOGE("failed to write file");
            return 1;
        }
    }
    free(buf);
    close(fd_in);
    close(fd_out);
    return 0;
}
