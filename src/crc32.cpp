#include <stdio.h>
#include <vector>
#include "crc32.h"
#include "utils.h"

int main(int argc, const char **argv) {
    if (argc < 2) {
        return 1;
    }
    std::vector<char> data;
    if (read_all(argv[1], data) != 0) {
        LOGE("failed to read file");
        return 1;
    }
    printf("%u\n", crc32_8bytes(data.data(), data.size(), 0));
    return 0;
}
