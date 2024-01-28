# Bundle interpreter

Feel free to bundle the interpreter and all it's dependencies along with your binary.

To use the bundled interpreter, use a shebang with relative path.

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
ssc ./test_bash_static.sh binary -s

# create tarball (or AppImage if you like)
chmod +x binary bash-static
tar -zcvf release.tar.gz binary bash-static
```

## Example: busybox

`busybox` provides several Unix utilities in a single executable file. We can make a completely dependency-free app with it.

```bash
# download busybox
wget https://busybox.net/downloads/binaries/1.35.0-x86_64-linux-musl/busybox

# use -s flag to make our binary static too
ssc ./test_busybox.sh binary -s

# create tarball (or AppImage if you like)
chmod +x binary busybox
tar -zcvf release.tar.gz binary busybox
```
