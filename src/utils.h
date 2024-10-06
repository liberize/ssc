#pragma once
#include <string>
#include <unistd.h>
#include <limits.h>
#include <ftw.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif
#include "obfuscate.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define FORCE_INLINE __attribute__((always_inline)) inline

#define LOGD(fmt, ...) /* fprintf(stdout, fmt "\n", ##__VA_ARGS__) */
#define LOGE(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)

/*
 * These reporting functions use low-level I/O; on some systems, this
 * is a significant code reduction.  Of course, on many server and
 * desktop operating systems, malloc() and even crt rely on printf(),
 * which in turn pulls in most of the rest of stdio, so this is not an
 * optimization at all there.  (If you're going to pay 100k or more
 * for printf() anyway, you may as well use it!)
 */
FORCE_INLINE void msg(const char *m) {
    write(1, m, strlen(m));
}

FORCE_INLINE void errmsg(const char *m) {
    write(2, m, strlen(m));
}

FORCE_INLINE void errln(const char *m) {
    errmsg(m);
    errmsg("\n");
}

FORCE_INLINE std::string get_exe_path() {
    char buf[PATH_MAX] = {0};
#if defined(__linux__) || defined(__CYGWIN__)
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

FORCE_INLINE std::string str_replace_all(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
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
