# Embed an interpreter into the binary

We can embed the interpreter into our binary, and make a single-file dependency-free executable.

Another benefit from embedding is enhanced source protection. Without embedding, users can easily retrieve your script code with a spoofed interpreter.

To embed the interpreter, use `-e` flag.

The interpreter will be extracted to /tmp/ssc/XXXXXX, and be deleted after script execution. You may delete it like this at beginning of your script to avoid exposure of the interpreter:

```bash
rm -rf "$SSC_EXTRACT_DIR"
```

## Example: bash-static

`bash-static` is a static build of bash. It doesn't come with any dependencies, so it's suitable as an embedded interpreter.

```bash
# download bash-static
wget https://ftp.debian.org/debian/pool/main/b/bash/bash-static_5.2.15-2+b2_amd64.deb
ar x bash-static_5.2.15-2+b2_amd64.deb data.tar.xz
tar -xvf data.tar.xz ./bin/bash-static
mv ./bin/bash-static ./
rm -rf ./bin data.tar.xz bash-static_5.2.15-2+b2_amd64.deb

# use -s flag to make a static binary
../../ssc ./test_bash_static.sh binary -s -e bash-static

# test it
rm -f bash-static
./binary
```

## Example: busybox

`busybox` provides several Unix utilities in a single executable file. We can make a completely dependency-free binary with it.

```bash
# download busybox-static
wget https://ftp.debian.org/debian/pool/main/b/busybox/busybox-static_1.35.0-4+b3_amd64.deb
ar x busybox-static_1.35.0-4+b3_amd64.deb data.tar.xz
tar -xvf data.tar.xz ./bin/busybox
mv ./bin/busybox ./
rm -rf ./bin data.tar.xz busybox-static_1.35.0-4+b3_amd64.deb data.tar.xz

# use -s flag to make a static binary
../../ssc ./test_busybox.sh binary -s -e busybox

# test it
rm -f busybox
./binary
```
