#pragma once
#include <string>
#include <unistd.h>
#include <limits.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#include "obfuscate.h"

inline std::string get_exe_path() {
    char buf[PATH_MAX] = {0};
    int size = sizeof(buf);
#if defined(__linux__) || defined(__CYGWIN__)
    size = readlink(OBF("/proc/self/exe"), buf, size);
    return size == -1 ? std::string() : std::string(buf, size);
#elif defined(__APPLE__)
    return _NSGetExecutablePath(buf, (unsigned*) &size) ? std::string() : std::string(buf);
#else
    #error unsupported operating system!
#endif
}

inline std::string dir_name(const std::string& s) {
    return s.substr(0, s.find_last_of("\\/") + 1);
}

inline std::string base_name(const std::string& s) {
    return s.substr(s.find_last_of("\\/") + 1);
}

inline bool str_ends_with(const std::string& s, const std::string& e) {
    return s.size() >= e.size() && s.compare(s.size() - e.size(), e.size(), e) == 0;
}
