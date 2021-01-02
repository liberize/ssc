# Simple Shell Script Compiler

this is a simple tool to turn bash script to binary, inspired by shc.

sshc itself is not a compiler such as cc, it rather encodes a shell script and generates C++ source code. It then uses the system compiler to compile a binary which behaves exactly like the original script. Upon execution, the compiled binary will decode and pipe the script code to child bash process to execute.

# Usage

install gettext, g++ (5.2 or above), then

```bash
sshc script.sh binary
```

# Features and Limitations

* very simple, easy to understand
* ~~support bash only for now~~ support shebang, see below
* simple code protection with compile time obfuscation rather than encryption
* no anti-debugging features
* pipes shell code to bash to avoid command line exposure

# Which Shell will be Used

| if                                 | script.sh has no shebang        | script.sh begins with a shebang<br />`#!/path/to/shell args` |
|------------------------------------|---------------------------------|--------------------------------------------------------------|
| `./binary`        is equivalent to | `cat script.sh \| sh`           | `cat script.sh \| /path/to/shell args`                       |
| `./binary params` is equivalent to | `cat script.sh \| sh -s params` | `cat script.sh \| /path/to/shell args -s params`             |

PS: `-s` flag works for dash, bash, ksh, zsh, csh, tcsh...
