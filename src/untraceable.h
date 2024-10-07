#pragma once
#include "obfuscate.h"
#include "utils.h"

#if defined(__CYGWIN__)
#include <Windows.h>

FORCE_INLINE void check_debugger() {
    if (IsDebuggerPresent()) {
        LOGD("debugger present!");
        exit(1);
    }
}

#elif defined(__APPLE__)
#include <sys/sysctl.h>

FORCE_INLINE void check_debugger() {
    struct kinfo_proc info;
    info.kp_proc.p_flag = 0;
    size_t size = sizeof(info);
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
    if (sysctl(mib, 4, &info, &size, nullptr, 0) == 0 &&
        (info.kp_proc.p_flag & P_TRACED) != 0) {
        LOGD("debugger present!");
        exit(1);
    }
}

#elif defined(__linux__) || defined(__FreeBSD__)
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <unistd.h>
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

FORCE_INLINE void check_debugger() {
#ifdef __linux__
    std::ifstream ifs(OBF("/proc/self/status"));
    std::string line, needle = OBF("TracerPid:\t");
    int tracer_pid = 0;
    while (std::getline(ifs, line)) {
        auto idx = line.find(needle);
        if (idx != std::string::npos) {
            tracer_pid = atoi(line.c_str() + idx + needle.size());
            break;
        }
    }
    ifs.close();
    if (tracer_pid != 0) {
        LOGD("found tracer. tracer_pid=%d", tracer_pid);
        exit(1);
    }
    std::ifstream ifs2(OBF("/proc/sys/kernel/yama/ptrace_scope"));
    int ptrace_scope = 0;
    ifs2 >> ptrace_scope;
    ifs2.close();
    if (getuid() != 0 && ptrace_scope != 0) {
        LOGD("skip ptrace detection. uid=%d ptrace_scope=%d", getuid(), ptrace_scope);
        return;
    }
#endif
    int ppid = getpid();
    int p = fork();
    if (p < 0) {
        LOGE("fork failed");
        exit(1);
    } else if (p > 0) { // parent process
        waitpid(p, 0, 0);
    } else {
        if (ptrace(PT_ATTACHEXC, ppid, 0, 0) == 0) {
            wait(0);
            ptrace(PT_DETACH, ppid, 0, 0);
        } else {
            LOGD("being traced!");
            kill(ppid, SIGKILL);
        }
        _Exit(0);
    }
}
#endif
