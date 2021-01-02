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
#include "obfuscate.h"

int main(int argc, char* argv[])
{
    const char* script = AY_OBFUSCATE(R"SSHC(${SCRIPT_CONTENT})SSHC");

    // use 3 pipes to replace stdin, stdout and stderr of child process
    int fd_in[2], fd_out[2], fd_err[2];
    if (pipe(fd_in) == -1 || pipe(fd_out) == -1 || pipe(fd_err) == -1) {
        perror("create pipe failed");
        return 1;
    }

    auto ppid = getpid();
    auto p = fork();
    if (p < 0) {
        perror("fork failed");
        return 2;
    } else if (p > 0) { // parent process
        close(fd_in[0]);
        close(fd_out[1]);
        close(fd_err[1]);
        
        // write script content to writing end of fd_in pipe, then close it
        write(fd_in[1], script, strlen(script));
        close(fd_in[1]);

        // read from reading end of fd_out/fd_err pipe, then write to parent stdout/stderr
        char buf[1024];
        size_t nread_out, nread_err;
        do {
            nread_out = nread_err = 0;
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(fd_out[0], &fds);
            FD_SET(fd_err[0], &fds);
            auto maxfd = std::max(fd_out[0], fd_err[0]);
            auto ret = select(maxfd + 1, &fds, NULL, NULL, NULL);
            if (ret < 0) {
                break;
            }
            if (FD_ISSET(fd_out[0], &fds)) {
                if ((nread_out = read(fd_out[0], buf, sizeof(buf))) > 0) {
                    write(STDOUT_FILENO, buf, nread_out);
                }
            }
            if (FD_ISSET(fd_err[0], &fds)) {
                if ((nread_err = read(fd_err[0], buf, sizeof(buf))) > 0) {
                    write(STDERR_FILENO, buf, nread_err);
                }
            }
        } while (nread_out > 0 || nread_err > 0);
        close(fd_out[0]);
        close(fd_err[0]);
        
        // wait for child to exit
        wait(NULL);
        return 0;
    } else { // child process
        // make sure child dies if parent die
        if (prctl(PR_SET_PDEATHSIG, SIGTERM) == -1) {
            return 1;
        }
        if (getppid() != ppid) {
            return 2;
        }

        close(fd_in[1]);
        dup2(fd_in[0], STDIN_FILENO); // replace stdin with reading end of fd_in pipe
        close(fd_in[0]);

        close(fd_out[0]);
        dup2(fd_out[1], STDOUT_FILENO); // replace stdout with writing end of fd_out pipe
        close(fd_out[1]);

        close(fd_err[0]);
        dup2(fd_err[1], STDERR_FILENO); // replace stderr with writing end of fd_err pipe
        close(fd_err[1]);

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
        }
        if (argc == 1) {
            if (args.empty()) {
                args.emplace_back("sh"); // default to sh to support more shells
            }
        } else {
            if (args.empty()) {
                args.emplace_back("bash"); // default to bash because we need '-s' flag
            }
            args.emplace_back("-s");
            for (auto i = 1; i < argc; i++) {
                args.emplace_back(argv[i]);
            }
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
    }
}