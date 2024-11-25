#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "rc4.h"

int main(int argc, const char **argv) {
    if (argc != 4) {
        return 1;
    }
    int fd_in = open(argv[1], O_RDONLY);
    if (fd_in == -1) {
        LOGE("open input file failed");
        return 1;
    }
    off_t size = lseek(fd_in, 0, SEEK_END);
    lseek(fd_in, 0, SEEK_SET);
    if (size == (off_t) -1) {
        LOGE("seek failed");
        return 1;
    }
    char *buf = (char*) malloc(size);
    if (!buf) {
        LOGE("malloc failed");
        return 1;
    }
    if (read(fd_in, buf, size) != size) {
        LOGE("read file failed");
        return 1;
    }
    close(fd_in);
    rc4((u8*) buf, size, (u8*) argv[3], strlen(argv[3]));
    int fd_out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out == -1) {
        LOGE("open output file failed");
        return 1;
    }
    if (write(fd_out, buf, size) != size) {
        LOGE("write file failed");
        return 1;
    }
    free(buf);
    close(fd_out);
    return 0;
}
