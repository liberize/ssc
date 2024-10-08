#define _LINUX_SOURCE_COMPAT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <wordexp.h>
#include <vector>
#include <string>
#include <iterator>
#include <algorithm>
#include "obfuscate.h"
#include "utils.h"
#ifdef __linux__
#include <sys/mount.h>
#endif

#ifdef UNTRACEABLE
#include "untraceable.h"
#endif
#if defined(EMBED_INTERPRETER_NAME) || defined(EMBED_ARCHIVE) || defined(RC4_KEY)
#include "embed.h"
#endif
#ifdef MOUNT_SQUASHFS
#include "mount.h"
#endif
#ifdef RC4_KEY
#include "rc4.h"
#endif

enum ScriptFormat {
    SHELL,
    PYTHON,
    PERL,
    JAVASCRIPT,
    RUBY,
    PHP,
    R,
    LUA,
};

int main(int argc, char* argv[]) {
#ifdef UNTRACEABLE
    check_debugger();
#endif

#ifdef EXPIRE_DATE
    struct tm expire_tm;
    if (!strptime(OBF(STR(EXPIRE_DATE)), "%m/%d/%Y", &expire_tm)) {
        LOGE("invalid expire date!");
        return 1;
    }
    if (difftime(time(nullptr), mktime(&expire_tm)) >= 0) {
        LOGE(R"SSC(script has expired!)SSC");
        return 1;
    }
#endif

    std::string exe_path = get_exe_path(), base_dir = dir_name(exe_path);
    std::string interpreter_path, extract_dir, mount_dir;

#if defined(INTERPRETER)
    interpreter_path = OBF(STR(INTERPRETER));
#endif
#if defined(EMBED_INTERPRETER_NAME)
    interpreter_path = extract_embeded_file();
    extract_dir = dir_name(interpreter_path);
    atexit(remove_extract_dir);
#elif defined(EMBED_ARCHIVE)
    base_dir = extract_dir = extract_embeded_file();
    atexit(remove_extract_dir);
#elif defined(MOUNT_SQUASHFS)
    base_dir = mount_dir = mount_squashfs();
#endif
    setenv(OBF("SSC_EXTRACT_DIR"), extract_dir.c_str(), 1);
    setenv(OBF("SSC_MOUNT_DIR"), mount_dir.c_str(), 1);
    setenv(OBF("SSC_EXECUTABLE_PATH"), exe_path.c_str(), 1);
    setenv(OBF("SSC_ARGV0"), argv[0], 1);

    std::string script_name = OBF(R"SSC(SCRIPT_FILE_NAME)SSC");
#ifdef RC4_KEY
    const char* rc4_key = OBF(STR(RC4_KEY));
#ifdef __APPLE__
    auto buf = read_data_sect("s");
    if (buf.empty())
        exit(1);
    char* script_data =  buf.data();
    int script_len = buf.size();
#else
    extern char _binary_s_start;
    extern char _binary_s_end;
    char* script_data = &_binary_s_start;
    int script_len = &_binary_s_end - &_binary_s_start;
#endif
    rc4((u8*) script_data, script_len, (u8*) rc4_key, strlen(rc4_key));
    const char* script = script_data;
#else
    const char* script = OBF(R"SSC(SCRIPT_CONTENT)SSC");
    int script_len = strlen(script);
#endif
    // support utf8 with bom
    if (script_len >= 3 && script[0] == '\xEF' && script[1] == '\xBB' && script[2] == '\xBF') {
        script += 3;
        script_len -= 3;
    }

    // detect script format by file name suffix
    ScriptFormat format = SHELL;
    std::string shell("sh");
    auto pos = script_name.find_last_of(".");
    if (pos != std::string::npos) {
        auto suffix = script_name.substr(pos + 1);
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
        } else if (suffix == "r") {
            format = R;
        } else if (suffix == "lua") {
            format = LUA;
        }
    }

    std::vector<std::string> args;
    const char *shebang_end = script;
    // parse shebang
    if (script[0] == '#' && script[1] == '!') {
        std::string line;
        auto p = strpbrk(script, "\r\n");
        if (p) {
            line.assign(script + 2, p - script - 2);
            shebang_end = p + (p[0] == '\r' && p[1] == '\n' ? 2 : 1);
        } else {
            line.assign(script + 2);
            shebang_end = script + script_len;
        }

        wordexp_t wrde;
        if (wordexp(line.c_str(), &wrde, 0) != 0) {
            LOGE("failed to parse shebang!");
            return 1;
        }
        for (size_t i = 0; i < wrde.we_wordc; i++) {
            auto s = wrde.we_wordv[i];
            if (args.empty()) {
                if (!strcmp(s, "env") || !strcmp(s, "/usr/bin/env")) {
                    continue;
                }
                for (p = s; *p == '_' || isalnum(*p); ++p);
                if (*p == '=') {
                    std::string name(s, p - s);
                    setenv(name.c_str(), ++p, 1);
                    continue;
                }
            }
            args.emplace_back(s);
        }
        wordfree(&wrde);

        // detect script format by shebang
        if (!args.empty()) {
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
            } else if (args[0].find("Rscript") != std::string::npos) {
                format = R;
            } else if (args[0].find("lua") != std::string::npos) {
                format = LUA;
            }
        }
    }
    if (args.empty()) {
        switch (format) {
            case SHELL:      args.emplace_back(shell); break;
            case PYTHON:     args.emplace_back("python"); break;
            case PERL:       args.emplace_back("perl"); break;
            case JAVASCRIPT: args.emplace_back("node"); break;
            case RUBY:       args.emplace_back("ruby"); break;
            case PHP:        args.emplace_back("php"); break;
            case R:          args.emplace_back("Rscript"); break;
            case LUA:        args.emplace_back("lua"); break;
            default:         LOGE("unknown format!"); return 4;
        }
    }
    if (interpreter_path.empty()) {
        // support relative path
        pos = args[0].find('/');
        if (pos != std::string::npos && pos != 0) {
            interpreter_path = base_dir + args[0];
        } else {
            interpreter_path = args[0];
        }
    }
    setenv(OBF("SSC_INTERPRETER_PATH"), interpreter_path.c_str(), 1);
    
#ifdef __FreeBSD__
    char fifo_name[PATH_MAX];
    int l = 100;
    for (int i = getpid(); l--; ) {
        i = (i * 1436856257) % 1436856259;
        sprintf(fifo_name, OBF("%s/ssc.%08x"), tmpdir(), i);
        if (!mkfifo(fifo_name, S_IWUSR | S_IRUSR))
            break;
    }
    if (l < 0) {
        LOGE("failed to create fifo!");
        return 2;
    }
#else
    int fd_script[2];
    if (pipe(fd_script) == -1) {
        LOGE("failed to create pipe!");
        return 2;
    }
#endif

    int ppid = getpid();
    int p = fork();
    if (p < 0) {
        LOGE("failed to fork process!");
        return 1;
    } else if (p > 0) { // parent process

#ifdef __FreeBSD__
        std::string path = fifo_name;
#else
        close(fd_script[1]);
        
        std::string path = OBF("/proc/self/fd");
        struct stat st;
#ifdef __linux__
        if ((stat(path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) && getuid() == 0) {
            mkdir(OBF("/proc"), 0755);
            mount(OBF("none"), OBF("/proc"), OBF("proc"), 0, nullptr);
        }
#endif
        if (stat(OBF("/dev/fd"), &st) == 0 && S_ISDIR(st.st_mode)) {
            path = OBF("/dev/fd");
        }
        path += '/';
        path += std::to_string(fd_script[0]);
#endif

        if (format == JAVASCRIPT) {
            args.emplace_back(OBF("--preserve-symlinks-main"));
        }
#ifdef FIX_ARGV0
        if (format == SHELL) {
            args.emplace_back("-c");
            args.emplace_back(". " + path);
            args.emplace_back(argv[0]);
        } else
#endif
        args.emplace_back(path);

        for (auto i = 1; i < argc; i++) {
            args.emplace_back(argv[i]);
        }

        std::vector<const char*> cargs;
        cargs.reserve(args.size() + 1);
        for (const auto& arg : args) {
            cargs.push_back(arg.c_str());
        }
        cargs.push_back(NULL);
        execvp(interpreter_path.c_str(), (char* const*) cargs.data());
        // error in execvp
        LOGE("failed to execute interpreter! path=%s", interpreter_path.c_str());
        return 3;

    } else { // child process

#ifdef __FreeBSD__
        int fd = open(fifo_name, O_WRONLY);
        if (fd == -1) {
            LOGE("failed to open fifo for writing!");
            return 1;
        }
#else
        close(fd_script[0]);
        int fd = fd_script[1];
#endif

#ifdef FIX_ARGV0
        script_len -= shebang_end - script;
        script = shebang_end;
        if (format == SHELL) {
            if (shell == "bash") {
                // only bash 5+ support BASH_ARGV0
                //dprintf(fd, OBF("BASH_ARGV0='%s'\n"), str_replace_all(argv[0], "'", "'\\''").c_str());
            } else if (shell == "zsh") {
                dprintf(fd, OBF("0='%s'\n"), str_replace_all(argv[0], "'", "'\\''").c_str());
            } else if (shell == "fish") {
                dprintf(fd, OBF("set 0 '%s'\n"), str_replace_all(argv[0], "'", "'\\''").c_str());
            }
        } else if (format == PYTHON) {
            dprintf(fd, OBF("import sys; sys.argv[0] = '''%s'''\n"), argv[0]);
        } else if (format == PERL) {
            dprintf(fd, OBF("$0 = '%s';\n"), str_replace_all(argv[0], "'", "\\'").c_str());
        } else if (format == JAVASCRIPT) {
            dprintf(fd, OBF("__filename = `%s`; process.argv[1] = `%s`;\n"), argv[0], argv[0]);
        } else if (format == RUBY) {
            dprintf(fd, OBF("$PROGRAM_NAME = '%s'\n"), str_replace_all(argv[0], "'", "\\'").c_str());
        } else if (format == PHP) {
            dprintf(fd, OBF("<?php $argv[0] = '%s'; ?>\n"), str_replace_all(argv[0], "'", "\\'").c_str());
        } else if (format == LUA) {
            dprintf(fd, OBF("arg[0] = '%s'\n"), str_replace_all(argv[0], "'", "\\'").c_str());
        }
#endif
#ifdef UNTRACEABLE
        check_debugger();
#endif
        // write script content to writing end of fd_in pipe, then close it
        write(fd, script, script_len);
        close(fd);

#if defined(EMBED_INTERPRETER_NAME) || defined(EMBED_ARCHIVE) || defined(__FreeBSD__)
        // wait util parent process exit, then call atexit handler to remove extracted dir
        while (getppid() == ppid) {
            sleep(1);
        }
#endif
#ifdef __FreeBSD__
        unlink(fifo_name);
#endif
    }
}
