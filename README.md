# Simple Script Compiler

This is a simple tool to turn script to binary, inspired by shc.

ssc itself is not a compiler such as cc, it rather generates c++ source code with script code, then uses c++ compiler to compile a binary which behaves exactly like the original script.

Upon execution, the compiled binary will call real script interpreter (systemwide, bundled or embeded), and fork a child process to pipe script code to the interpreter to execute.

# Prerequisite

* g++ (5.2 or above)
* perl, binutils (probably already installed)
* libarchive-dev, acl-dev, libz-dev (only required by -E flag)
* libandroid-wordexp (if you're using android termux)

# Usage

```bash
./ssc script binary
```

More options

```
Usage: ./ssc [-4] [-u] [-s] [-r] [-e|-E file] [-0] <script> <binary>
  -u, --untraceable        make untraceable binary
                           enable debugger detection, abort program when debugger is found
  -s, --static             make static binary
                           link statically, binary is more portable but bigger
  -4, --rc4                encrypt script with rc4 instead of compile time obfuscation
  -r, --random-key         use random key for obfuscation and encryption
  -i, --interpreter        override interpreter path
                           the interpreter will be used no matter what shebang is
  -e, --embed-interpreter  embed specified interpreter into binary
                           the interpreter will be used no matter what shebang is
  -E, --embed-archive      embed specified tar.gz archive into binary, require libarchive-dev
                           set relative path in shebang to use an interpreter in the archive
  -0, --fix-argv0          try to fix $0, may not work
                           if it doesn't work or causes problems, use $SSC_ARGV0 instead
  -v, --verbose            show debug messages
  -h, --help               display this help and exit
```

# Features

* support Linux/macOS/Cygwin
* **support Shell/Python/Perl/NodeJS/Ruby/PHP/R/Lua** and other scripts with custom shebang
* support relative path, environment variable and variable expanding in shebang
* code protection with **compile time obfuscation or rc4 encryption**
* pipes script code to interpreter to **avoid command line exposure**
* support large script, up to 8MB with compile time obfuscation and unlimited with rc4 encryption
* **anti-debugging** with ptrace detection
* support embeding an interpreter or archive into output binary

# Limitations

* `$0` / `$ARGV[0]` / `sys.argv[0]` is replaced by /dev/fd/xxx. Try `-0` flag or use `$SSC_ARGV0` instead.

# Builtin variables

The following builtin variables are available to the script (including shebang):

* `SSC_INTERPRETER_PATH`: actual interpreter path
* `SSC_EXECUTABLE_PATH`: current executable path
* `SSC_ARGV0`: first command line argument (i.e. $0)
* `SSC_EXTRACT_DIR`: temporary extraction directory for embeded file, if -e or -E flag is used

# Interpreter selection

If the script has no shebang, it's format will be deduced from file extension, and a default interpreter in PATH will be used.

If the script has a shebang, the shebang will be used to launch an interpreter process.

If the script has a relative-path shebang, the interpreter of the path relative to the binary will be used. 

If the binary is generated with `-i`, the interpreter path specified after `-i` will be used to launch an interpreter process according to the shebang. In this case, the program specified in the shebang will appear as process name, but not be used actually.

If the binary is generated with `-e`, the interpreter is built into the binary. Upon execution, the interpreter will be extracted to /tmp/ssc/XXXXXX/, then be used to launch an interpreter process according to the shebang. In this case, the program specified in the shebang will appear as process name, but not be used actually.

If the binary is generated with `-E`, the archive is built into the binary. Upon execution, the archive will be decompressed and extracted to /tmp/ssc/XXXXXX/ with permissions perserved. If the script has a relative-path shebang, the interpreter of the path relative to the extraction directory will be used, otherwise, a system intepreter will be used.

# Cross compiling

Set `CROSS_COMPILE` variable just like using Makefile.

For example, to compile arm64 binary on x86_64 ubuntu:

```bash
apt install g++-aarch64-linux-gnu binutils-aarch64-linux-gnu
CROSS_COMPILE=aarch64-linux-gnu- ./ssc script binary
```
