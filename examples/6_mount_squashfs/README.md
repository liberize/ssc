# Embed a squashfs file and mount it at runtime

This is another way to pack some data into the binary and make a dependency-free executable. Same method is used by AppImage. Currently only availble on Linux.

To embed a squashfs file, use `-M` flag. If a directory is specified, `mksquashfs` is called to create a squashfs file from the directory.

The squashfs file is directly appended to the binary, upon execution it will be mounted to /tmp/ssc/XXXXXX with FUSE. This method doesn't extract files, so it is generally faster than `-E` flag.

## Example: python

We can embed a whole python package to our binary.

```bash
# download standalone python
wget https://github.com/indygreg/python-build-standalone/releases/download/20240814/cpython-3.10.14+20240814-x86_64-unknown-linux-gnu-install_only_stripped.tar.gz -O cpython.tar.gz

# delete redundant files in the archive
tar -zxvf cpython.tar.gz
rm -rf python/include python/share python/lib/pkgconfig python/bin/{2to3*,idle*,pip*,pydoc*,*-config}
rm -rf python/lib/{*tcl*,thread*,Tix*,tk*}

# compile script to binary, create a squashfs file and append it to the binary
../../ssc ./test_python.sh binary -M python
rm -f python

# test it
./binary
```
