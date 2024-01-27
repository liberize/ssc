# Simple Script Compiler

This is a simple tool to turn script to binary, inspired by shc.

ssc itself is not a compiler such as cc, it rather generates c++ source code with the script code, then uses the system compiler to compile a binary which behaves exactly like the original script.

Upon execution, the compiled binary will replace current process with real script interpreter process, and fork a child process to pipe the script code to parent process to execute.

**This tool doesn't generate standalone binary. A script interpreter (systemwide, bundled or embeded) is neccessary for the binary to run.**

# Usage

Install g++ (5.2 or above), then

```bash
./ssc script binary
```

More options

```
Usage: ./ssc [-u] [-s] [-r] [-e interpreter] <script> <binary>
  -u, --untraceable        make untraceable binary
  -s, --static             make static binary, more portable but bigger
  -r, --random-key         use random key for obfuscation
  -e, --embed-interpreter  embed specified interpreter into binary
  -v, --verbose            show debug messages
  -h, --help               display this help and exit
```

# Features

* support Linux/macOS/Cygwin
* **support shell/python/perl/nodejs/ruby/php** or other scripts with custom shebang
* support relative path in shebang, so it's possible to call a bundled interpreter
* simple code protection with **compile time obfuscation**
* pipes script code to interpreter to **avoid command line exposure**
* support large script (up to 8MB)
* **anti-debugging** with simple ptrace detection
* support embeding interpreter to output binary

# Limitations

* `$0` / `$ARGV[0]` / `sys.argv[0]` is replaced by /dev/fd/xxx. use `$SSC_SCRIPT_NAME` or `$SSC_BINARY_NAME` instead.

# Interpreter selection

If the script has no shebang, it's format is deduced from file extension, and a default interpreter in PATH is used.

If the script has a shebang, the shebang will be used to launch an interpreter process.

If the program in the shebang is a relative path, the interpreter of the path relative to the binary is used. 

If the binary is generated with `-e`, the interpreter is built into the binary. Upon execution, the interpreter will be extracted to /tmp/ssc/, then be used to launch an interpreter process according to the shebang. In this case, the program specified in the shebang will appear as process name, but not be used actually.
