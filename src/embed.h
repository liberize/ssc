#pragma once
#include <string>
#include <vector>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "utils.h"
#include "rc4.h"
#ifdef EMBED_ARCHIVE
#include "untar.h"
#endif

#ifdef __APPLE__
#include <mach-o/getsect.h>

FORCE_INLINE std::vector<char> read_data_sect(const char *name) {
    const struct section_64 *sect = getsectbyname("binary", name);
    if (!sect) {
        LOGE("failed to find data section");
        return std::vector<char>();
    }
    std::vector<char> buf(sect->size, '\0');
    int fd_exe = open(get_exe_path().c_str(), O_RDONLY);
    if (fd_exe == -1) {
        LOGE("failed to open executable file");
        return std::vector<char>();
    }
    lseek(fd_exe, sect->offset, SEEK_SET);
    if (read(fd_exe, buf.data(), sect->size) != sect->size) {
        LOGE("failed to read data section");
        return std::vector<char>();
    }
    close(fd_exe);
    return buf;
}
#endif

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

    const char* rc4_key = OBF(STR(RC4_KEY));
    rc4((u8*) data, size, (u8*) rc4_key, strlen(rc4_key));
    
    char path[PATH_MAX];
    strcpy(path, tmpdir());
    strcat(path, OBF("/ssc.XXXXXX"));
    if (!mkdtemp(path)) {
        LOGE("failed to create output directory");
        exit(1);
    }
    strcat(path, "/");

#if defined(EMBED_INTERPRETER_NAME)
    strcat(path, base_name(STR(EMBED_INTERPRETER_NAME)).c_str());
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd == -1) {
        LOGE("failed to open output file");
        exit(1);
    }
    if (write(fd, data, size) != size) {
        LOGE("failed to write output file");
        exit(1);
    }
    close(fd);
    if (chmod(path, 0755) == -1) {
        LOGE("failed to chmod 755");
        exit(1);
    }
#elif defined(EMBED_ARCHIVE)
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        LOGE("failed to get current dir");
        exit(1);
    }
    if (chdir(path) == -1) {
        LOGE("failed to change dir");
        exit(1);
    }
    if (extract_tar_gz_from_mem(data, size) != 0)
        exit(1);
    if (chdir(cwd) == -1) {
        LOGE("failed to change back dir");
        exit(1);
    }
#endif
    return path;
}
