# Simple Script Compiler

This is a powerful tool to turn any script to binary, inspired by shc.

ssc itself is not a compiler such as cc, it rather generates c++ source code with script code, then uses c++ compiler to compile the source into a binary which behaves exactly like the original script.

Upon execution, the binary will call real script interpreter (systemwide, bundled or embeded), and fork a child process to pipe script code to the interpreter to execute.

## Quickstart

```bash
echo "echo 'hello ssc'" >test.sh
docker run --rm -v $PWD:/workspace liberize/ssc:latest -s test.sh test
./test
```

## Prerequisite

*(Note: g++ version should be 5.2 or above)*

<details>
<summary>For Debian-based Linux distros</summary>
<p>

* g++, perl, binutils
* libc-dev, libstdc++-dev (only required by -s flag)
* libz-dev (only required by -E flag)
* libz-dev, libfuse-dev, git, gcc, make, automake, autoconf, pkg-config, libtool, squashfs-tools (only required by -M flag)

</p>
</details>

<details>
<summary>For RedHat-based Linux distros</summary>
<p>

* g++, perl, binutils
* glibc-static, libstdc++-static (only required by -s flag)
* zlib-devel (only required by -E flag)
* zlib-devel, fuse-devel, git, gcc, make, automake, autoconf, pkgconfig, libtool, squashfs-tools (only required by -M flag)

</p>
</details>

<details>
<summary>For macOS</summary>
<p>

* Xcode command line tools

</p>
</details>

<details>
<summary>For Android Termux</summary>
<p>

* g++, perl, binutils, libandroid-wordexp
* libandroid-wordexp-static, ndk-multilib-native-static (only required by -s flag)

</p>
</details>

<details>
<summary>For Cygwin</summary>
<p>

* gcc-g++, perl, binutils

</p>
</details>

<details>
<summary>For FreeBSD</summary>
<p>

* gcc, binutils

</p>
</details>

## Usage

```bash
./ssc script binary
```

More options

```
Usage: ./ssc [-4] [-u] [-s] [-r] [-e|-E|-M file] [-0] [-d date] [-m msg] <script> <binary>
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
  -E, --embed-archive      embed specified tar.gz archive into binary, require zlib
                           set relative path in shebang to use an interpreter in the archive
  -M, --mount-squashfs     append specified gzipped squashfs to binary and mount it at runtime
                           linux only, works like AppImage. may specify a directory
  -0, --fix-argv0          try to fix $0, may not work
                           if it doesn't work or causes problems, use $SSC_ARGV0 instead
  -d, --expire-date        expire date, for example, 11/30/2023
  -m, --expire-message     expire message, default to 'script has expired!'
                           if it's a valid file path, read message from the file
  -v, --verbose            show debug messages
  -h, --help               display this help and exit
```

## Features

* support Linux/macOS/Cygwin/FreeBSD
* **support Shell/Python/Perl/NodeJS/Ruby/PHP/R/Lua** and other scripts with custom shebang
* support relative path, environment variable and variable expanding in shebang
* code protection with **compile time obfuscation or rc4 encryption**
* pipes script code to interpreter to **avoid command line exposure**
* support large script, up to 8MB with compile time obfuscation and unlimited with rc4 encryption
* **anti-debugging** with ptrace detection
* support **embeding an interpreter or archive** into output binary

## Limitations

* `$0` / `$ARGV[0]` / `sys.argv[0]` is replaced by /dev/fd/xxx or /tmp/xxxxxx. Try `-0` flag or use `$SSC_ARGV0` instead.

## Examples

1. [Script without a shebang.](https://github.com/liberize/ssc/tree/master/examples/1_without_shebang)
2. [Script with a shebang.](https://github.com/liberize/ssc/tree/master/examples/2_with_shebang)
3. [Call a bundled interpreter.](https://github.com/liberize/ssc/tree/master/examples/3_bundle_interpreter)
4. [Embed an interpreter into the binary.](https://github.com/liberize/ssc/tree/master/examples/4_embed_interpreter)
5. [Embed an archive into the binary.](https://github.com/liberize/ssc/tree/master/examples/5_embed_archive)
6. [Embed a squashfs file and mount it at runtime.](https://github.com/liberize/ssc/tree/master/examples/6_mount_squashfs)

## Builtin variables

The following builtin variables are available to the script (including shebang):

* `SSC_INTERPRETER_PATH`: actual interpreter path
* `SSC_EXECUTABLE_PATH`: current executable path
* `SSC_ARGV0`: first command line argument (i.e. $0)
* `SSC_EXTRACT_DIR`: temporary extraction directory for embeded file, if -e or -E flag is used
* `SSC_MOUNT_DIR`: temporary mount directory for squashfs, if -M flag is used

## Interpreter selection

If the script has no shebang, it's format will be deduced from file extension, and a default interpreter in PATH will be used.

If the script has a shebang, the shebang will be used to launch an interpreter process.

If the script has a relative-path shebang, the interpreter of the path relative to the binary will be used. 

If the binary is generated with `-i`, the interpreter path specified after `-i` will be used to launch an interpreter process according to the shebang. In this case, the program specified in the shebang will appear as process name, but not be used actually.

If the binary is generated with `-e`, the interpreter is built into the binary. Upon execution, the interpreter will be extracted to /tmp/ssc/XXXXXX/, then be used to launch an interpreter process according to the shebang. In this case, the program specified in the shebang will appear as process name, but not be used actually.

If the binary is generated with `-E`, the archive is built into the binary. Upon execution, the archive will be decompressed and extracted to /tmp/ssc/XXXXXX/ with permissions perserved. If the script has a relative-path shebang, the interpreter of the path relative to the extraction directory will be used, otherwise, a system intepreter will be used.

If the binary is generated with `-M`, the squashfs file is appended to the binary. Upon execution, the squashfs file will be mounted to /tmp/ssc/XXXXXX/. If the script has a relative-path shebang, the interpreter of the path relative to the mount directory will be used, otherwise, a system intepreter will be used.

## Cross compiling

Set `CROSS_COMPILE` variable just like using Makefile.

For example, to compile arm64 binary on x86_64 ubuntu:

```bash
apt install g++-aarch64-linux-gnu binutils-aarch64-linux-gnu
CROSS_COMPILE=aarch64-linux-gnu- ./ssc script binary
```

## Disclaimer

Please read carefully and fully understand the following disclaimer before using this software:

* This software is for personal study only, strictly prohibited for commercial or malicious purposes. if you use this software in the process of any illegal behavior, you need to bear the corresponding consequences, the author of the software will not bear any legal responsibility or joint liability.

* This software is shared free of charge, any loss caused by using this software will be entirely borne by you, the author of the software will not bear any responsibility.

* The copyright of this software statement, its right of modification, update and final interpretation belong to the author of the software.

* Using this software means that you have fully understood, recognized and accepted all the terms and contents in this disclaimer.
