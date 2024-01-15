# Simple Script Compiler

This is a simple tool to turn shell/python/perl script to binary, inspired by shc.

ssc itself is not a compiler such as cc, it rather encodes a shell/python/perl script and generates c++ source code. It then uses the system compiler to compile a binary which behaves exactly like the original script. Upon execution, the compiled binary will decode and pipe the script code to real shell/python/perl process to execute.

**This tool doesn't generate standalone binary. A script interpreter is neccessary for the binary to run.**

# Usage

Install gettext, g++ (5.2 or above), then

```bash
ssc script.sh binary
ssc script.py binary
ssc script.pl binary
```

# Features and Limitations

* **support shell/python/perl**
* **support shebang**
* simple code protection with **compile time obfuscation**
* pipes script code to interpreter to **avoid command line exposure**
* no anti-debugging features

# How it Works

It forks a child process to pipe script content to parent process.

Parent process reads from /proc/self/fd/<PIPE_FD>, so stdin is not replaced.
