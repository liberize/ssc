# Simple Script Compiler

This is a simple tool to turn shell/python/perl script to binary, inspired by shc.

ssc itself is not a compiler such as cc, it rather encodes a shell/python/perl script and generates c++ source code. It then uses the system compiler to compile a binary which behaves exactly like the original script. Upon execution, the compiled binary will decode and pipe the script code to child shell/python/perl process to execute.

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

For shell script:

| if                                 | script.sh has no shebang        | script.sh begins with a shebang<br />`#!/path/to/shell args` |
|------------------------------------|---------------------------------|--------------------------------------------------------------|
| `./binary`        is equivalent to | `cat script.sh \| sh`           | `cat script.sh \| /path/to/shell args`                       |
| `./binary params` is equivalent to | `cat script.sh \| sh -s params` | `cat script.sh \| /path/to/shell args -s params`             |

PS: `-s` flag works for dash, bash, ksh, zsh, csh, tcsh...

For python (and perl) script:

| if                                 | script.py has no shebang           | script.py begins with a shebang<br />`#!/usr/bin/env python` |
|------------------------------------|------------------------------------|---------------------------------------------------------------|
| `./binary`        is equivalent to | `cat script.py \| python`          | `cat script.py \| /usr/bin/env python`                       |
| `./binary params` is equivalent to | `cat script.py \| python - params` | `cat script.py \| /usr/bin/env python - params`              |

Script format is guessed by file name extension and shebang.
