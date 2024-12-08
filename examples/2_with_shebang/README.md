# Compile a script with a shebang

File extension is not required if a shebang is present.

Free to use any commands in shebang.

```bash
# shebang is required if no file extension
../../ssc test binary
# it's ok to use /usr/bin/env in shebang
../../ssc test.py binary
# use any command in shebang even it's not listed as supported
../../ssc test.expect binary
```
