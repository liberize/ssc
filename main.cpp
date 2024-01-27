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

#define OBF(x) (const char *) AY_OBFUSCATE(x)

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
    char buf[PATH_MAX];
    int size = sizeof(buf);
#if defined(__linux__) || defined(__CYGWIN__)
    size = readlink(OBF("/proc/self/exe"), buf, size);
    return size == -1 ? std::string() : std::string(buf, size);
#elif defined(__APPLE__)
    return _NSGetExecutablePath(buf, (unsigned*) &size) ? std::string() : std::string(buf, size);
#else
    #error unsupported operating system!
#endif
}

inline std::string dir_name(const std::string& s) {
    return s.substr(0, s.find_last_of('/') + 1);
}

inline std::string base_name(const std::string& s) {
    return s.substr(s.find_last_of('/') + 1);
}

inline bool str_ends_with(const std::string& s, const std::string& e) {
    return s.size() >= e.size() && s.compare(s.size() - e.size(), e.size(), e) == 0;
}

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
        
        std::string scriptPath(file_name), binaryPath = get_exe_path();
        setenv("SSC_SCRIPT_NAME", base_name(scriptPath).c_str(), 1);
        setenv("SSC_BINARY_NAME", base_name(binaryPath).c_str(), 1);
        
        // detect script format by file name suffix
        ScriptFormat format = SHELL;
        std::string shell("sh");
        auto pos = scriptPath.find_last_of(".");
        if (pos != std::string::npos) {
            auto suffix = scriptPath.substr(pos + 1);
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
                auto exe = (args[0] == "/usr/bin/env" && args.size() > 1) ? args[1] : args[0];
                if (str_ends_with(exe, "sh")) {
                    format = SHELL;
                    shell = base_name(exe);
                } else if (exe.find("python") != std::string::npos || exe.find("conda") != std::string::npos) {
                    format = PYTHON;
                } else if (exe.find("perl") != std::string::npos) {
                    format = PERL;
                } else if (exe.find("node") != std::string::npos) {
                    format = JAVASCRIPT;
                } else if (exe.find("ruby") != std::string::npos) {
                    format = RUBY;
                } else if (exe.find("php") != std::string::npos) {
                    format = PHP;
                }
                // support relative path
                pos = args[0].find('/');
                if (pos != std::string::npos && pos != 0) {
                    args[0] = dir_name(binaryPath) + args[0];
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
        execvp(cargs[0], (char* const*)cargs.data());
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