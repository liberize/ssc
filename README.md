# Simple Script Compiler

This is a simple tool to turn script to binary, inspired by shc.

ssc itself is not a compiler such as cc, it rather generates c++ source code with the script code, then uses the system compiler to compile a binary which behaves exactly like the original script.

Upon execution, the compiled binary will replace current process with real script interpreter process, and fork a child process to pipe the script code to parent process to execute.

**This tool doesn't generate standalone binary. A script interpreter (systemwide or bundled) is neccessary for the binary to run.**

# Usage

Install g++ (5.2 or above) or clang++, then

```bash
./ssc script binary
./ssc -u script binary  # untraceable binary, requires root to run on linux
```

**Tips: you should run ssc on an OS with old enough glibc for portability of the generated binary.**

# Features

* **support shell/python/perl/nodejs/ruby**
* support perhaps any shebang
* simple code protection with **compile time obfuscation**
* pipes script code to interpreter to **avoid command line exposure**
* support large script (up to 8MB)
* **anti-debugging** with simple ptrace detection

# Limitations

* \$0 / $ARGV[0] / sys.argv[0] is replaced by /dev/fd/xxx
