# Simple Script Compiler

此工具可以将几乎任何脚本转换为二进制文件，灵感来自于shc。

ssc本身并不是一个编译器，比如cc，它会生成包含脚本代码的C++源代码，然后使用C++编译器将源代码编译成一个二进制文件，该二进制文件的行为与原始脚本完全相同。

在执行时，二进制文件会调用真实的脚本解释器（系统的、外带的或嵌入的），并创建一个子进程，将脚本代码通过管道传递给解释器执行。

## 依赖包

*(注意：g++版本应为5.2或以上)*

<details>
<summary>基于Debian的Linux发行版</summary>
<p>

* g++, perl, binutils
* libc-dev, libstdc++-dev（仅在使用-s选项时需要）
* libz-dev（仅在使用-E选项时需要）
* libz-dev, libfuse-dev, git, gcc, make, automake, autoconf, pkg-config, libtool, squashfs-tools（仅在使用-M选项时需要）

</p>
</details>

<details>
<summary>基于RedHat的Linux发行版</summary>
<p>

* g++, perl, binutils
* glibc-static, libstdc++-static（仅在使用-s选项时需要）
* zlib-devel（仅在使用-E选项时需要）
* zlib-devel, fuse-devel, git, gcc, make, automake, autoconf, pkgconfig, libtool, squashfs-tools（仅在使用-M选项时需要）

</p>
</details>

<details>
<summary>macOS</summary>
<p>

* Xcode命令行工具

</p>
</details>

<details>
<summary>Android Termux</summary>
<p>

* g++, perl, binutils, libandroid-wordexp
* libandroid-wordexp-static, ndk-multilib-native-static（仅在使用-s选项时需要）

</p>
</details>

<details>
<summary>Cygwin</summary>
<p>

* gcc-g++, perl, binutils

</p>
</details>

<details>
<summary>FreeBSD</summary>
<p>

* gcc, binutils

</p>
</details>

## 使用方法

```bash
./ssc script binary
```

更多选项

```
./ssc [-u] [-s] [-r] [-e|-E|-M file] [-0] [-d date] [-m msg] [-S N] <script> <binary>

  -u, --untraceable        生成不可追踪的二进制文件
                           启用调试器检测，发现调试器时中止程序
  -s, --static             生成静态二进制文件
                           使用静态链接，二进制文件更具可移植性，但体积更大
  -r, --random-key         使用随机密钥进行rc4加密
  -i, --interpreter        强制指定解释器路径
                           无论shebang是什么，都会使用指定的解释器
  -e, --embed-interpreter  将指定的解释器嵌入二进制文件
                           无论shebang是什么，都会使用嵌入的解释器
  -E, --embed-archive      将指定的tar.gz压缩包嵌入二进制文件
                           在shebang中使用相对路径以使用压缩包中的解释器
  -M, --mount-squashfs     将指定的gzip压缩的squashfs文件追加到二进制文件中，并在运行时挂载
                           仅适用于Linux，类似AppImage。如果指定的是目录，从这个目录创建squashfs文件
  -0, --fix-argv0          尝试修复$0，可能不起作用
                           如果不起作用或造成问题，请尝试使用-n选项或使用$SSC_ARGV0代替$0
  -n, --ps-name            更改ps输出中的脚本路径
                           这将创建一个指向真实路径的符号链接并将这个链接传递给解释器
  -d, --expire-date        过期日期，例如 11/30/2023
  -m, --expire-message     过期提示，默认为 script has expired!
                           如果是有效的文件路径，则从该文件读取过期提示
  -S, --segment            将脚本分割成多个片段，默认为1个片段
                           在执行时，依次解密并写入脚本片段，在写入每个片段之前检测调试器
  -v, --verbose            显示调试信息
  -h, --help               显示帮助并退出
```

## 特性

* 支持Linux/macOS/Android/Cygwin/FreeBSD
* 支持Shell/Python/Perl/NodeJS/Ruby/PHP/R/Lua以及其它自定义shebang的脚本
* 支持shebang中使用相对路径、环境变量和变量展开
* 使用rc4加密保护源代码，密钥经过编译时混淆
* 通过管道传输脚本代码到解释器，以避免命令行暴露脚本内容
* 脚本长度没有限制
* 反调试，检测当前进程是否被附加了调试器
* 支持将解释器或压缩包嵌入到输出的二进制文件

## 限制

* `$0` / `$ARGV[0]` / `sys.argv[0]` 会被替换为 /dev/fd/xxx 或 /tmp/xxxxxx。可以尝试使用-0选项，或使用-n选项，或者使用`$SSC_ARGV0`替代`$0`。

## 示例

1. [没有shebang的脚本。](https://github.com/liberize/ssc/tree/master/examples/1_without_shebang)
2. [有shebang的脚本。](https://github.com/liberize/ssc/tree/master/examples/2_with_shebang)
3. [调用外带的解释器。](https://github.com/liberize/ssc/tree/master/examples/3_bundle_interpreter)
4. [将解释器嵌入二进制文件。](https://github.com/liberize/ssc/tree/master/examples/4_embed_interpreter)
5. [将压缩包嵌入二进制文件。](https://github.com/liberize/ssc/tree/master/examples/5_embed_archive)
6. [将squashfs文件嵌入并在运行时挂载。](https://github.com/liberize/ssc/tree/master/examples/6_mount_squashfs)

## 源代码保护

使用下面这些选项以最大限度保护源代码不被泄露：
```
./ssc script binary -u -s -r -e /path/to/interpreter -S N
```

* 使用编译时混淆技术来混淆字符串字面量，这样使用`strings`命令时不会显示任何有用的信息。
* 移除所有调试符号，增加了反编译和分析二进制文件的难度。
* -u选项在运行时启用调试器检测，阻止像`gdb`、`strace`这样的工具追踪`write`系统调用。
* -s选项编译静态二进制文件，避免了LD_PRELOAD钩子。
* -r选项生成随机的RC4密钥（编译时混淆过），增加了直接从二进制文件中使用密钥解密的难度。
* -e选项将解释器嵌入到二进制文件中（RC4加密过），防止使用伪造的解释器获取源代码。
* -S选项将脚本分割成N个片段，在将每个片段写入管道之前检测调试器和读取管道的进程，这样通过读取管道或转储内存最多得到一个片段。

## 内置变量

以下内置变量在脚本中可用（包括shebang）：

* `SSC_INTERPRETER_PATH`: 实际的解释器路径
* `SSC_EXECUTABLE_PATH`: 当前可执行文件的路径
* `SSC_ARGV0`: 第一个命令行参数（即`$0`）
* `SSC_EXTRACT_DIR`: 嵌入文件的临时提取目录（如果使用了-e或-E选项）
* `SSC_MOUNT_DIR`: squashfs文件的临时挂载目录（如果使用了-M选项）

## 解释器的选择

如果脚本没有shebang，将根据文件扩展名来推测脚本格式，并使用PATH环境变量中的默认解释器。

如果脚本有shebang，将使用shebang中的解释器路径和命令行参数。

如果脚本使用了相对路径的shebang，将使用相对于当前二进制文件路径的解释器。

如果二进制文件是通过-i生成的，将使用-i后指定的解释器，并使用shebang中的命令行参数。这种情况下，shebang中指定的程序将作为进程名称出现，但实际用的是-i后指定的解释器。

如果二进制文件是通过-e生成的，解释器将被嵌入到二进制文件中。执行时，解释器将被提取到/tmp/ssc/XXXXXX/目录中，然后使用shebang中的命令行参数来启动解释器。这种情况下，shebang中指定的程序将作为进程名称出现，但实际用的是嵌入的解释器。

如果二进制文件是通过-E生成的，压缩包将被嵌入到二进制文件中。执行时，压缩包会被解压并提取到/tmp/ssc/XXXXXX/目录中，并保持文件权限。如果脚本使用了相对路径的shebang，将使用相对于提取目录的解释器；否则，将使用系统默认的解释器。

如果二进制文件是通过-M生成的，squashfs文件将被附加到二进制文件中。执行时，squashfs文件会被挂载到/tmp/ssc/XXXXXX/目录中。如果脚本使用了相对路径的shebang，将使用相对于挂载目录的解释器；否则，将使用系统默认的解释器。

## 交叉编译

像使用Makefile一样设置`CROSS_COMPILE`变量即可。

例如，要在x86_64 Ubuntu上编译arm64二进制文件：

```bash
apt install g++-aarch64-linux-gnu binutils-aarch64-linux-gnu
CROSS_COMPILE=aarch64-linux-gnu- ./ssc script binary
```

## 免责声明

请仔细阅读并充分理解以下免责声明后使用本软件：

* 本软件仅供个人学习使用，严禁用于商业或恶意目的。如果您在使用本软件的过程中出现任何违法行为，您需要承担相应的后果，软件作者不承担任何法律责任或连带责任。

* 本软件免费共享，因使用本软件造成的任何损失将由您完全承担，软件作者不承担任何责任。

* 本软件版权声明、修改权、更新权以及最终解释权归属于软件作者。

* 使用本软件意味着您已经完全理解、认可并接受了免责声明中的所有条款和内容。
