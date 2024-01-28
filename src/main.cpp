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
#include "obfuscate.h"
#include "utils.h"

#ifdef UNTRACEABLE
#include "untraceable.h"
#endif
#ifdef EMBED_INTERPRETER_NAME
#include "embed.h"
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
        setenv("SSC_EXECUTABLE_PATH", exePath.c_str(), 1);
        setenv("SSC_INTERPRETER_PATH", interpreterPath.c_str(), 1);
        
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