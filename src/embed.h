#pragma once
#include <string>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <ftw.h>

#ifdef __APPLE__
#include <mach-o/getsect.h>
#else
extern char _binary___start;
extern char _binary___end;
#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

static int remove_file(const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb) {
    remove(pathname);
    return 0;
}

inline std::string extract_interpreter() {
    auto dir = OBF("/tmp/ssc");
    nftw(dir, remove_file, 10, FTW_DEPTH | FTW_MOUNT | FTW_PHYS);
    mkdir(dir, 0755);
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/XXXXXX", dir);
    if (!mkdtemp(path)) {
        perror(OBF("create output directory failed"));
        _exit(1);
    }
    strcat(path, "/");
    strcat(path, base_name(STR(EMBED_INTERPRETER_NAME)).c_str());
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd == -1) {
        perror(OBF("open output file failed"));
        _exit(1);
    }
#ifdef __APPLE__
    const struct section_64 *sect = getsectbyname("binary", "_");
    if (!sect) {
        perror(OBF("find data section failed"));
        _exit(1);
    }
    char *buf = new char[sect->size];
    int fd2 = open(get_exe_path().c_str(), O_RDONLY);
    if (fd2 == -1) {
        perror(OBF("open executable file failed"));
        _exit(1);
    }
    lseek(fd2, sect->offset, SEEK_SET);
    if (read(fd2, buf, sect->size) != sect->size) {
        perror(OBF("read data section failed"));
        _exit(1);
    }
    close(fd2);
    if (write(fd, buf, sect->size) != sect->size) {
        perror(OBF("write output file failed"));
        _exit(1);
    }
    delete[] buf;
#else
    auto size = &_binary___end - &_binary___start;
    if (write(fd, &_binary___start, size) != size) {
        perror(OBF("write output file failed"));
        _exit(1);
    }
#endif
    close(fd);
    chmod(path, 0755);
    return path;
}
