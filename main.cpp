#define _LINUX_SOURCE_COMPAT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <vector>
#include <string>
#include <sstream>
#include <iterator>
#include <algorithm>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#include "obfuscate.h"

#ifdef OBFUSCATE_KEY
    #define OBF(x) (const char *) AY_OBFUSCATE_KEY(x, OBFUSCATE_KEY)
#else
    #define OBF(x) (const char *) AY_OBFUSCATE(x)
#endif

#ifdef UNTRACEABLE
#if defined(__CYGWIN__)
#include <Windows.h>

inline void check_debugger() {
    if (IsDebuggerPresent()) {
        //perror(OBF("debugger present!"));
        _exit(1);
    }
}

#elif defined(__linux__) || defined(__APPLE__)
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <fstream>

#if !defined(PT_ATTACHEXC) /* New replacement for PT_ATTACH */
    #if defined(PTRACE_ATTACH)
        #define PT_ATTACHEXC PTRACE_ATTACH
    #elif defined(PT_ATTACH)
        #define PT_ATTACHEXC PT_ATTACH
    #endif
#endif
#if !defined(PT_DETACH)
    #if defined(PTRACE_DETACH)
        #define PT_DETACH PTRACE_DETACH
    #endif
#endif

inline void check_debugger() {
#ifdef __linux__
    std::ifstream ifs(OBF("/proc/self/status"));
    std::string line, needle = OBF("TracerPid:\t");
    int tracerPid = 0;
    while (std::getline(ifs, line)) {
        auto idx = line.find(needle);
        if (idx != std::string::npos) {
            tracerPid = atoi(line.c_str() + idx + needle.size());
            break;
        }
    }
    ifs.close();
    if (tracerPid != 0) {
        //fprintf(stderr, OBF("found tracer. tracerPid=%d\n"), tracerPid);
        _exit(1);
    }
    std::ifstream ifs2(OBF("/proc/sys/kernel/yama/ptrace_scope"));
    int ptraceScope = 0;
    ifs2 >> ptraceScope;
    ifs2.close();
    if (getuid() != 0 && ptraceScope != 0) {
        //fprintf(stderr, OBF("skip ptrace detection. uid=%d ptraceScope=%d\n"), getuid(), ptraceScope);
        return;
    }
#endif
    int ppid = getpid();
    int p = fork();
    if (p < 0) {
        perror(OBF("fork failed"));
        _exit(1);
    } else if (p > 0) { // parent process
        waitpid(p, 0, 0);
    } else {
        if (ptrace(PT_ATTACHEXC, ppid, 0, 0) == 0) {
            wait(0);
            ptrace(PT_DETACH, ppid, 0, 0);
        } else {
            //perror(OBF("being traced!"));
            kill(ppid, SIGKILL);
        }
        _exit(0);
    }
}
#endif
#endif

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

#ifdef EMBED_INTERPRETER_NAME
#include <fstream>
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
#endif

enum ScriptFormat {
    SHELL,
    PYTHON,
    PERL,
    JAVASCRIPT,
    RUBY,
    PHP,
};

int main(int argc, char* argv[]) {
#ifdef UNTRACEABLE
    check_debugger();
#endif
    std::string interpreterPath;
#ifdef EMBED_INTERPRETER_NAME
    interpreterPath = extract_interpreter();
    setenv("PATH", (dir_name(interpreterPath) + ':' + getenv("PATH")).c_str(), 1);
#endif

    const char* file_name = OBF(R"SSC(SCRIPT_FILE_NAME)SSC");
    const char* script = OBF(R"SSC(SCRIPT_CONTENT)SSC");

    int fd_script[2];
    if (pipe(fd_script) == -1) {
        perror(OBF("create pipe failed"));
        return 2;
    }

    int p = fork();
    if (p < 0) {
        perror(OBF("fork failed"));
        return 1;
    } else if (p > 0) { // parent process
        close(fd_script[1]);
        
        std::string scriptName(file_name), exePath = get_exe_path();
        setenv("SSC_EXE_PATH", exePath.c_str(), 1);
        
        // detect script format by file name suffix
        ScriptFormat format = SHELL;
        std::string shell("sh");
        auto pos = scriptName.find_last_of(".");
        if (pos != std::string::npos) {
            auto suffix = scriptName.substr(pos + 1);
            std::transform(suffix.begin(), suffix.end(), suffix.begin(), [] (unsigned char c) {
                return std::tolower(c);
            });
            if (str_ends_with(suffix, "sh")) {
                format = SHELL;
                shell = suffix;
            } else if (suffix == "py" || suffix == "pyw") {
                format = PYTHON;
            } else if (suffix == "pl") {
                format = PERL;
            } else if (suffix == "js") {
                format = JAVASCRIPT;
            } else if (suffix == "rb") {
                format = RUBY;
            } else if (suffix == "php") {
                format = PHP;
            }
        }

        std::vector<std::string> args;
        // parse shebang
        if (script[0] == '#' && script[1] == '!') {
            std::string line;
            auto p = strpbrk(script, "\r\n");
            if (p) {
                line.assign(script + 2, p - script - 2);
            } else {
                line.assign(script + 2);
            }
            std::istringstream iss(line);
            //FIXME: handle quotes and spaces (maybe use wordexp?)
            args.assign(std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{});
            
            // detect script format by shebang
            if (!args.empty()) {
                if (args[0] == "/usr/bin/env" && args.size() > 1) {
                    args.erase(args.begin());
                }
                if (str_ends_with(args[0], "sh")) {
                    format = SHELL;
                    shell = base_name(args[0]);
                } else if (args[0].find("python") != std::string::npos || args[0].find("conda") != std::string::npos) {
                    format = PYTHON;
                } else if (args[0].find("perl") != std::string::npos) {
                    format = PERL;
                } else if (args[0].find("node") != std::string::npos) {
                    format = JAVASCRIPT;
                } else if (args[0].find("ruby") != std::string::npos) {
                    format = RUBY;
                } else if (args[0].find("php") != std::string::npos) {
                    format = PHP;
                }
                // support relative path
                pos = args[0].find('/');
                if (pos != std::string::npos && pos != 0) {
                    args[0] = dir_name(exePath) + args[0];
                }
            }
        }
        if (args.empty()) {
            switch (format) {
                case SHELL:      args.emplace_back(shell); break; // default to 'sh'
                case PYTHON:     args.emplace_back("python"); break; // default to 'python'
                case PERL:       args.emplace_back("perl"); break; // default to 'perl'
                case JAVASCRIPT: args.emplace_back("node"); break; // default to 'node'
                case RUBY:       args.emplace_back("ruby"); break; // default to 'ruby'
                case PHP:        args.emplace_back("php"); break; // default to 'php'
                default:         perror(OBF("unknown format")); return 4;
            }
        }
        if (format == JAVASCRIPT) {
            args.emplace_back("--preserve-symlinks-main");
        }
        args.emplace_back(OBF("/dev/fd/") + std::to_string(fd_script[0]));
        for (auto i = 1; i < argc; i++) {
            args.emplace_back(argv[i]);
        }
        
        std::vector<const char*> cargs;
        cargs.reserve(args.size() + 1);
        for (const auto& arg : args) {
            cargs.push_back(arg.c_str());
        }
        cargs.push_back(NULL);
        execvp(interpreterPath.empty() ? cargs[0] : interpreterPath.c_str(), (char* const*) cargs.data());
        // error in execvp
        perror(OBF("execvp failed"));
        return 3;

    } else { // child process

        // write script content to writing end of fd_in pipe, then close it
        close(fd_script[0]);
        write(fd_script[1], script, strlen(script));
        close(fd_script[1]);

        return 0;
    }
}