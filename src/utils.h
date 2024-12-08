#pragma once
#include <string>
#include <vector>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <ftw.h>
#if defined(__linux__)
#include <dirent.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__FreeBSD__)
#include <sys/sysctl.h>
#elif defined(__CYGWIN__)
#include <Windows.h>
#endif
#include "obfuscate.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define FORCE_INLINE __attribute__((always_inline)) inline

#if defined(__clang__)
#define DISABLE_OPTIMIZATION [[clang::optnone]]
#elif defined(__GNUC__)
#define DISABLE_OPTIMIZATION __attribute__((optimize("O0")))
#else
#define DISABLE_OPTIMIZATION
#endif

#define LOGD(fmt, ...)  /* fprintf(stdout, OBF(fmt "\n"), ##__VA_ARGS__) */
#define LOGE(fmt, ...)  fprintf(stderr, OBF(fmt "\n"), ##__VA_ARGS__)

FORCE_INLINE int is_big_endian() {
    int i = 1;
    return ! *((char*) &i);
}

FORCE_INLINE std::string get_exe_path() {
    char buf[PATH_MAX] = {0};
#if defined(__linux__)
    int size = sizeof(buf);
    size = readlink(OBF("/proc/self/exe"), buf, size);
    return size == -1 ? std::string() : std::string(buf, size);
#elif defined(__APPLE__)
    unsigned size = sizeof(buf);
    return _NSGetExecutablePath(buf, &size) ? std::string() : std::string(buf);
#elif defined(__FreeBSD__)
    size_t size = sizeof(buf);
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, getpid() };
    return sysctl(mib, 4, buf, &size, nullptr, 0) ? std::string() : std::string(buf);
#elif defined(__CYGWIN__)
    return !GetModuleFileNameA(NULL, buf, sizeof(buf)) ? std::string() : std::string(buf);
#else
    #error unsupported operating system!
#endif
}

FORCE_INLINE std::string dir_name(const std::string& s) {
    return s.substr(0, s.find_last_of("\\/") + 1);
}

FORCE_INLINE std::string base_name(const std::string& s) {
    return s.substr(s.find_last_of("\\/") + 1);
}

FORCE_INLINE bool str_ends_with(const std::string& s, const std::string& e) {
    return s.size() >= e.size() && s.compare(s.size() - e.size(), e.size(), e) == 0;
}

FORCE_INLINE bool str_contains_word(const std::string& s, const std::string& w) {
    auto p = s.find(w);
    if (p == std::string::npos)
        return false;
    if (p && isalpha(s[p-1]))
        return false;
    p += w.size();
    if (p < s.size() && isalpha(s[p]))
        return false;
    return true;
}

FORCE_INLINE std::string str_replace_all(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

FORCE_INLINE int read_all(const char* path, std::vector<char>& buf) {
    auto fd = open(path, O_RDONLY);
    if (fd == -1) {
        LOGE("failed to open file. path=%s", path);
        return -1;
    }
    auto size = lseek(fd, 0, SEEK_END);
    if (size == -1) {
        LOGE("failed to seek to end of file. path=%s", path);
        close(fd);
        return -1;
    }
    lseek(fd, 0, SEEK_SET);
    buf.resize(size);
    if (read(fd, buf.data(), size) != size) {
        LOGE("failed to read file. path=%s", path);
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

static int _remove_file(const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb) {
    remove(pathname);
    return 0;
}

FORCE_INLINE void remove_directory(const char *dir) {
    nftw(dir, _remove_file, 10, FTW_DEPTH | FTW_PHYS);
}

FORCE_INLINE bool is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

FORCE_INLINE bool is_symlink(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISLNK(st.st_mode);
}

static int mkdir_recursive(const char *dir, int mode = S_IRWXU) {
    char tmp[PATH_MAX + 1];
    strncpy(tmp, dir, PATH_MAX);
    size_t len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (char *p = tmp + 1; *p; p++)
        if (*p == '/') {
            *p = 0;
            if (!is_dir(tmp) && mkdir(tmp, mode) != 0)
                return -1;
            *p = '/';
        }
    if (!is_dir(tmp) && mkdir(tmp, mode) != 0)
        return -1;
    return 0;
}

FORCE_INLINE const char* tmpdir() {
    auto d = getenv("TMPDIR");
    return d && d[0] ? d : "/tmp";
}

#ifdef __linux__
FORCE_INLINE void check_pipe_reader(int fd) {
    static auto mypid = getpid(), ppid = getppid();
    static char pipe_dst[128] = {0};

    char path[PATH_MAX];
    int size = 0;
    if (!pipe_dst[0]) {
        snprintf(path, sizeof(path), OBF("/proc/self/fd/%d"), fd);
        if ((size = readlink(path, pipe_dst, sizeof(pipe_dst) - 1)) < 0)
            return;
        pipe_dst[size] = '\0';
    }

    auto proc_dir = opendir(OBF("/proc"));
    if (!proc_dir)
        return;

    char link_dst[PATH_MAX];
    struct dirent *entry;
    while ((entry = readdir(proc_dir))) {
        if (entry->d_type != DT_DIR)
            continue;
        char *end = nullptr;
        auto pid = strtoul(entry->d_name, &end, 10);
        if (!pid || *end)
            continue;
        if (pid == mypid || pid == ppid)
            continue;
        
        auto len = snprintf(path, sizeof(path), OBF("/proc/%s/fd"), entry->d_name);
        auto fd_dir = opendir(path);
        if (!fd_dir)
            continue;
        path[len++] = '/';
        
        while ((entry = readdir(fd_dir))) {
            if (entry->d_type != DT_LNK)
                continue;
            strcpy(path + len, entry->d_name);
            if ((size = readlink(path, link_dst, sizeof(link_dst) - 1)) < 0)
                continue;
            link_dst[size] = '\0';
            if (strcmp(link_dst, pipe_dst) == 0) {
                LOGD("process %lu is reading our pipe!", pid);
                sleep(5);
                exit(1);
            }
        }
        closedir(fd_dir);
    }

    closedir(proc_dir);
}
#endif

class AutoCleaner {
public:
    ~AutoCleaner() {
        for (auto& path : m_paths) {
            if (is_dir(path.c_str())) {
                remove_directory(path.c_str());
            } else {
                unlink(path.c_str());
            }
        }
    }
    void add(std::string path) {
        m_paths.emplace_back(std::move(path));
    }
private:
    std::vector<std::string> m_paths;
};
