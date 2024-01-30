#pragma once
#include <string>
#include <vector>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "utils.h"
#ifdef EMBED_ARCHIVE
#include "untar.h"
#endif

#ifdef __APPLE__
#include <mach-o/getsect.h>
#else
extern char _binary___start;
extern char _binary___end;
#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

inline std::string extract_embeded_file() {
#ifdef __APPLE__
    const struct section_64 *sect = getsectbyname("binary", "_");
    if (!sect) {
        perror(OBF("find data section failed"));
        _exit(1);
    }
    size_t size = sect->size;
    std::vector<char> buf(size, '\0');
    char *data = buf.data();
    int fd_exe = open(get_exe_path().c_str(), O_RDONLY);
    if (fd_exe == -1) {
        perror(OBF("open executable file failed"));
        _exit(1);
    }
    lseek(fd_exe, sect->offset, SEEK_SET);
    if (read(fd_exe, data, size) != size) {
        perror(OBF("read data section failed"));
        _exit(1);
    }
    close(fd_exe);
#else
    char *data = &_binary___start;
    size_t size = &_binary___end - &_binary___start;
#endif
    
    auto dir = OBF("/tmp/ssc");
    // delete the whole directory can cause troubles when multiple instances are running
    //remove_directory(dir);
    mkdir(dir, 0755);
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/XXXXXX", dir);
    if (!mkdtemp(path)) {
        perror(OBF("create output directory failed"));
        _exit(1);
    }
    strcat(path, "/");

#if defined(EMBED_INTERPRETER_NAME)
    strcat(path, base_name(STR(EMBED_INTERPRETER_NAME)).c_str());
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd == -1) {
        perror(OBF("open output file failed"));
        _exit(1);
    }
    if (write(fd, data, size) != size) {
        perror(OBF("write output file failed"));
        _exit(1);
    }
    close(fd);
    if (chmod(path, 0755) == -1) {
        perror(OBF("chmod 755 failed"));
        _exit(1);
    }
#elif defined(EMBED_ARCHIVE)
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror(OBF("get current dir failed"));
        _exit(1);
    }
    if (chdir(path) == -1) {
        perror(OBF("change dir failed"));
        _exit(1);
    }
    extract_from_mem(data, size);
    if (chdir(cwd) == -1) {
        perror(OBF("change back dir failed"));
        _exit(1);
    }
#endif
    return path;
}

void remove_extract_dir() {
    auto extract_dir = getenv("SSC_EXTRACT_DIR");
    if (extract_dir && !strncmp(extract_dir, "/tmp/", 5)) {
        //fprintf(stderr, "remove %s\n", extract_dir);
        remove_directory(extract_dir);
    }
}
