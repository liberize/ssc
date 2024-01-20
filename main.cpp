#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/prctl.h>

#include <vector>
#include <string>
#include <sstream>
#include <iterator>
#include <algorithm>
#include "obfuscate.h"

enum ScriptFormat {
    SHELL,
    PYTHON,
    PERL,
    JAVASCRIPT,
    RUBY,
};

int main(int argc, char* argv[])
{
    const char* file_name = AY_OBFUSCATE(R"SSC(SCRIPT_FILE_NAME)SSC");
    const char* script = AY_OBFUSCATE(R"SSC(SCRIPT_CONTENT)SSC");

    int fd_script[2];
    if (pipe(fd_script) == -1) {
        perror("create pipe failed");
        return 1;
    }

    auto ppid = getpid();
    auto p = fork();
    if (p < 0) {
        perror("fork failed");
        return 2;
    } else if (p > 0) { // parent process
        close(fd_script[1]);
        
        // detect script format by file name suffix
        ScriptFormat format = SHELL;
        std::string name(file_name);
        auto suffix = name.substr(name.find_last_of(".") + 1);
        std::transform(suffix.begin(), suffix.end(), suffix.begin(), [](unsigned char c){ return std::tolower(c); });
        if (suffix == "py" || suffix == "pyw") {
            format = PYTHON;
        } else if (suffix == "pl") {
            format = PERL;
        } else if (suffix == "js") {
            format = JAVASCRIPT;
        } else if (suffix == "rb") {
            format = RUBY;
        }

        std::vector<std::string> args;
        // parse shebang
        if (script[0] == '#' && script[1] == '!') {
            std::string line;
            auto p = strpbrk(script, "\r\n");
            if (p) {
                line = std::string(script + 2, p - script - 2);
            } else {
                line = std::string(script + 2);
            }
            std::istringstream iss(line);
            args = std::vector<std::string>{std::istream_iterator<std::string>{iss},
                std::istream_iterator<std::string>{}};
            
            // detect script format by shebang
            if (!args.empty()) {
                auto exe = (args[0] == "/usr/bin/env" && args.size() > 1) ? args[1] : args[0];
                if (exe.compare(exe.size() - 2, 2, "sh") == 0) {
                    format = SHELL;
                } else if (exe.find("python") != std::string::npos || exe.find("conda") != std::string::npos) {
                    format = PYTHON;
                } else if (exe.find("perl") != std::string::npos) {
                    format = PERL;
                } else if (exe.find("node") != std::string::npos) {
                    format = JAVASCRIPT;
                } else if (exe.find("ruby") != std::string::npos) {
                    format = RUBY;
                }
            }
        }
        if (args.empty()) {
            switch (format) {
                case SHELL:      args.emplace_back("sh"); break; // default to 'sh'
                case PYTHON:     args.emplace_back("python"); break; // default to 'python'
                case PERL:       args.emplace_back("perl"); break; // default to 'perl'
                case JAVASCRIPT: args.emplace_back("node"); break; // default to 'node'
                case RUBY:       args.emplace_back("ruby"); break; // default to 'ruby'
                default:         perror("unknown format"); return 4;
            }
        }
        if (format == JAVASCRIPT) {
            args.emplace_back("--preserve-symlinks-main");
        }
        args.emplace_back("/dev/fd/" + std::to_string(fd_script[0]));
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
        perror("execvp failed");
        return 3;

    } else { // child process
        // make sure child dies if parent die
        if (prctl(PR_SET_PDEATHSIG, SIGTERM) == -1) {
            return 1;
        }
        if (getppid() != ppid) {
            return 2;
        }

        // write script content to writing end of fd_in pipe, then close it
        close(fd_script[0]);
        write(fd_script[1], script, strlen(script));
        close(fd_script[1]);

        return 0;
    }
}