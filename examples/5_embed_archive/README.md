# Embed archive

We can even embed an archive into our binary, and make a self-extracting dependency-free executable.

To embed an archive, use `-E` flag. The archive may or may not contain an interpreter.

The interpreter will be extracted to /tmp/ssc/XXXXXX, and is not deleted by default. You have to delete it in your script like this:

```bash
rm -rf "$SSC_EXTRACT_DIR"
```

## Example: python

We can embed a whole python package to our binary.

```bash
# download standalone python
wget https://github.com/indygreg/python-build-standalone/releases/download/20240107/cpython-3.10.13+20240107-x86_64-unknown-linux-gnu-install_only.tar.gz -O cpython.tar.gz

# delete redundant files in the archive to make extraction faster

# use -s flag to make our binary static too
ssc ./test_python.sh binary -s -E cpython.tar.gz

# test it
./binary
```
