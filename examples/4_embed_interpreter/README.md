# Embed interpreter

We can even embed the interpreter into our binary, and make a single-file denpendency-free executable.

Another benefit from embedding is enhanced source protection. Without embedding, users can easily retrieve your script code with a spoofed interpreter.

To embed the interpreter, use `-e` flag.

## Example: bash-static

`bash-static` is a static build of bash. It doesn't come with any dependencies, so it's easy to bundle it to our app.

```bash
# download bash-static
wget https://ftp.debian.org/debian/pool/main/b/bash/bash-static_5.2.15-2+b2_amd64.deb
ar x bash-static_5.2.15-2+b2_amd64.deb data.tar.xz
tar -xvf data.tar.xz ./bin/bash-static
mv ./bin/bash-static ./
rm -rf ./bin data.tar.xz bash-static_5.2.15-2+b2_amd64.deb

# use -s flag to make our binary static too
ssc ./test_bash_static.sh binary -s -e bash-static

# test it
rm -f bash-static
./binary
```

## Example: busybox

`busybox` provides several Unix utilities in a single executable file. We can make a completely dependency-free app with it.

```bash
# download busybox
wget https://busybox.net/downloads/binaries/1.35.0-x86_64-linux-musl/busybox

# use -s flag to make our binary static too
ssc ./test_busybox.sh binary -s -e busybox

# test it
rm -f busybox
./binary
```
