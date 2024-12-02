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
#include "embed.h"
#include "rc4.h"
#ifdef __linux__
#include <sys/mount.h>
#endif

#ifdef UNTRACEABLE
#include "untraceable.h"
#endif
#ifdef MOUNT_SQUASHFS
#include "mount.h"
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
    check_debugger(true);
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

    static AutoCleaner cleaner;
    std::string exe_path = get_exe_path(), base_dir = dir_name(exe_path);
    std::string interpreter_path, extract_dir, mount_dir;

#if defined(INTERPRETER)
    interpreter_path = OBF(STR(INTERPRETER));
#endif
#if defined(EMBED_INTERPRETER_NAME)
    interpreter_path = extract_embeded_file();
    extract_dir = dir_name(interpreter_path);
    cleaner.add(extract_dir);
#elif defined(EMBED_ARCHIVE)
    base_dir = extract_dir = extract_embeded_file();
    cleaner.add(extract_dir);
#elif defined(MOUNT_SQUASHFS)
    base_dir = mount_dir = mount_squashfs();
#endif
    setenv(OBF("SSC_EXTRACT_DIR"), extract_dir.c_str(), 1);
    setenv(OBF("SSC_MOUNT_DIR"), mount_dir.c_str(), 1);
    setenv(OBF("SSC_EXECUTABLE_PATH"), exe_path.c_str(), 1);
    setenv(OBF("SSC_ARGV0"), argv[0], 1);

    std::string file_name = OBF(R"SSC(SCRIPT_FILE_NAME)SSC");
    std::string shebang = OBF(R"SSC(SCRIPT_SHEBANG)SSC");

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

    // detect script format by file name suffix
    ScriptFormat format = SHELL;
    std::string shell("sh");
    auto pos = file_name.find_last_of(".");
    if (pos != std::string::npos) {
        auto suffix = file_name.substr(pos + 1);
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
    // parse shebang
    if (!shebang.empty()) {
        wordexp_t wrde;
        if (wordexp(shebang.c_str() + 2, &wrde, 0) != 0) {
            LOGE("failed to parse shebang!");
            return 1;
        }
        for (size_t i = 0; i < wrde.we_wordc; i++) {
            auto s = wrde.we_wordv[i];
            if (args.empty()) {
                if (!strcmp(s, "env") || !strcmp(s, "/usr/bin/env")) {
                    continue;
                }
                auto p = s;
                while (*p == '_' || isalnum(*p)) {
                    ++p;
                }
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
        if (!mkfifo(fifo_name, S_IWUSR | S_IRUSR)) {
            break;
        }
    }
    if (l < 0) {
        LOGE("failed to create fifo!");
        return 2;
    }
    std::string path = fifo_name;
    cleaner.add(path);
#else
    int fd_script[2];
    if (pipe(fd_script) == -1) {
        LOGE("failed to create pipe!");
        return 2;
    }
    std::string path = OBF("/proc/self/fd");
#ifdef __linux__
    if (!is_dir(path.c_str()) && getuid() == 0) {
        mkdir(OBF("/proc"), 0755);
        mount(OBF("none"), OBF("/proc"), OBF("proc"), 0, nullptr);
    }
#endif
    if (is_dir(OBF("/dev/fd"))) {
        path = OBF("/dev/fd");
    }
    path += '/';
    path += std::to_string(fd_script[0]);
#endif

#ifdef PS_NAME
    if (is_symlink(OBF(STR(PS_NAME)))) {
        unlink(OBF(STR(PS_NAME)));
    }
    if (symlink(path.c_str(), OBF(STR(PS_NAME))) != 0) {
        LOGE("failed to create symlink! path=" STR(PS_NAME) " err=`%s`", strerror(errno));
        return 5;
    }
    path = OBF(STR(PS_NAME));
    cleaner.add(path);
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

    int ppid = getpid();
    int p = fork();
    if (p < 0) {
        LOGE("failed to fork process!");
        return 1;
    } else if (p > 0) { // parent process

#ifndef __FreeBSD__
        close(fd_script[1]); 
#endif

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
        if (format == SHELL) {
            if (shell == "bash") {
                // only bash 5+ support BASH_ARGV0
                dprintf(fd, OBF("BASH_ARGV0='%s'\n"), str_replace_all(argv[0], "'", "'\\''").c_str());
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
#else
        write(fd, shebang.c_str(), shebang.size());
        write(fd, "\n", 1);
#endif

#ifdef __linux__
        auto pipe_id = get_pipe_id((OBF("/proc/self/fd/") + std::to_string(fd)).c_str());
#endif
        int n = SEGMENT;
        n = std::max(std::min(n, script_len), 1);
        int max_seg_len = (script_len + n - 1) / n;
        const char* rc4_key = OBF(STR(RC4_KEY));
        int rc4_key_len = strlen(rc4_key);
        while (script_len > 0) {
#ifdef UNTRACEABLE
            check_debugger(false);
#endif
#ifdef __linux__
            if (pipe_id > 0) {
                check_pipe_reader(pipe_id);
            }
#endif
            auto seg_len = std::min(max_seg_len, script_len);
            //LOGD("decrypt segment. size=%d", seg_len);
            rc4((u8*) script_data, seg_len, (u8*) rc4_key, rc4_key_len);
            write(fd, script_data, seg_len);
            memset(script_data, 0, seg_len);
            script_len -= seg_len;
            script_data += seg_len;
        }
        memset((void*) rc4_key, 0, rc4_key_len);
        close(fd);

#if defined(EMBED_INTERPRETER_NAME) || defined(EMBED_ARCHIVE) || defined(__FreeBSD__) || defined(PS_NAME)
        // wait util parent process exit
        signal(SIGINT, exit);
        while (getppid() == ppid) {
            sleep(1);
        }
#endif
    }
}
