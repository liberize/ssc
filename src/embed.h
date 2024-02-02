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

FORCE_INLINE std::vector<char> read_data_sect(const char *name) {
    const struct section_64 *sect = getsectbyname("binary", name);
    if (!sect) {
        perror(OBF("find data section failed"));
        return std::vector<char>();
    }
    std::vector<char> buf(sect->size, '\0');
    int fd_exe = open(get_exe_path().c_str(), O_RDONLY);
    if (fd_exe == -1) {
        perror(OBF("open executable file failed"));
        return std::vector<char>();
    }
    lseek(fd_exe, sect->offset, SEEK_SET);
    if (read(fd_exe, buf.data(), sect->size) != sect->size) {
        perror(OBF("read data section failed"));
        return std::vector<char>();
    }
    close(fd_exe);
    return buf;
}
#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)


FORCE_INLINE std::string extract_embeded_file() {
#ifdef __APPLE__
    auto buf = read_data_sect("i");
    if (buf.empty())
        exit(1);
    char *data =  buf.data();
    size_t size = buf.size();
#else
    extern char _binary_i_start;
    extern char _binary_i_end;
    char *data = &_binary_i_start;
    size_t size = &_binary_i_end - &_binary_i_start;
#endif
    
    auto dir = OBF("/tmp/ssc");
    // delete the whole directory can cause troubles when multiple instances are running
    //remove_directory(dir);
    mkdir(dir, 0755);
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/XXXXXX", dir);
    if (!mkdtemp(path)) {
        perror(OBF("create output directory failed"));
        exit(1);
    }
    strcat(path, "/");

#if defined(EMBED_INTERPRETER_NAME)
    strcat(path, base_name(STR(EMBED_INTERPRETER_NAME)).c_str());
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd == -1) {
        perror(OBF("open output file failed"));
        exit(1);
    }
    if (write(fd, data, size) != size) {
        perror(OBF("write output file failed"));
        exit(1);
    }
    close(fd);
    if (chmod(path, 0755) == -1) {
        perror(OBF("chmod 755 failed"));
        exit(1);
    }
#elif defined(EMBED_ARCHIVE)
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror(OBF("get current dir failed"));
        exit(1);
    }
    if (chdir(path) == -1) {
        perror(OBF("change dir failed"));
        exit(1);
    }
    extract_from_mem(data, size);
    if (chdir(cwd) == -1) {
        perror(OBF("change back dir failed"));
        exit(1);
    }
#endif
    return path;
}

static void remove_extract_dir() {
    auto extract_dir = getenv("SSC_EXTRACT_DIR");
    if (extract_dir && !strncmp(extract_dir, "/tmp/", 5)) {
        //fprintf(stderr, "remove %s\n", extract_dir);
        remove_directory(extract_dir);
    }
}
