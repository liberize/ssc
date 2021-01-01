#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/prctl.h>

#include <vector>
#include "obfuscate.h"

const char* g_script = AY_OBFUSCATE(R"SSHC(
${SCRIPT_CONTENT}
)SSHC");

int main(int argc, char** argv)
{
    int fd1[2];
    if (pipe(fd1)==-1)
    {
        perror("create pipe failed");
        return 1;
    }

    auto p = fork();
    if (p < 0)
    {
        perror("fork failed");
        return 1;
    }
    // parent process
    else if (p > 0)
    {
        close(fd1[0]);  // close reading end of pipe

        // write script content and close writing end of pipe
        write(fd1[1], g_script, strlen(g_script));
        close(fd1[1]);

        // wait for child to exit
        wait(NULL);
    }
    // child process
    else
    {
        // kill child after parent dies
        prctl(PR_SET_PDEATHSIG, SIGHUP);

        close(fd1[1]);  // close writing end of pipe
        dup2(fd1[0], 0);
        close(fd1[0]);

        std::vector<const char*> args(argc + 2);
        args[0] = "bash";
        args[1] = "-s";
        for (auto i = 1; i < argc; i++) {
            args[i + 1] = argv[i];
        }
        args[argc + 1] = NULL;
        execvp(args[0], (char* const*)args.data());
        // error in execvp
        perror("execvp failed");
        exit(1);
    }
}
