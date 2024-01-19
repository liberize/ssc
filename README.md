# Simple Script Compiler

This is a simple tool to turn shell/python/perl script to binary, inspired by shc.

ssc itself is not a compiler such as cc, it rather generates c++ source code with the script code, then uses the system compiler to compile a binary which behaves exactly like the original script.

Upon execution, the compiled binary will replace current process with real shell/python/perl process, and fork a child process to pipe the script code to parent process to execute.

**This tool doesn't generate standalone binary. A system script interpreter is neccessary for the binary to run.**

# Usage

Install g++ (5.2 or above), then

```bash
./ssc script.sh binary
./ssc script.py binary
./ssc script.pl binary
```

**Tips: you should run ssc on an OS with old enough glibc for portability of the generated binary.**

# Features and Limitations

* **support shell/python/perl**
* simple code protection with **compile time obfuscation**
* pipes script code to interpreter to **avoid command line exposure**
* support large script (up to 8MB)
* no anti-debugging features
