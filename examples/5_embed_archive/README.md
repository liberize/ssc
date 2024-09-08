# Embed an archive into the binary

We can even embed an archive into our binary, and make a self-extracting dependency-free executable.

To embed an archive, use `-E` flag. The archive may or may not contain an interpreter.

The interpreter will be extracted to /tmp/ssc/XXXXXX, and be deleted after script execution. You may delete it like this at beginning of your script to avoid exposure of the interpreter:

```bash
rm -rf "$SSC_EXTRACT_DIR"
```

## Example: python

We can embed a whole python package to our binary.

```bash
# download standalone python
wget https://github.com/indygreg/python-build-standalone/releases/download/20240107/cpython-3.10.13+20240107-x86_64-unknown-linux-gnu-install_only.tar.gz -O cpython.tar.gz

# delete redundant files in the archive to make extraction faster
tar -zxvf cpython.tar.gz
rm -rf python/include python/share python/lib/pkgconfig python/bin/{2to3*,idle*,pip*,pydoc*,*-config}
# rm -rf python/lib/{*tcl*,thread*,Tix*,tk*}
tar -zcvf cpython.tar.gz python
rm -rf python

# use -s flag to make our binary static too
../../ssc ./test_python.sh binary -s -E cpython.tar.gz

# test it
./binary
```
