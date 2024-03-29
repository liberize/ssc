#!busybox sh
#!sh
# both works

# remove extract dir /tmp/ssc/XXXXXX, this will delete all extracted files
# in this case, it's ok to remove it at the beginning of the script to avoid exposure of the interpreter
rm -rf "$SSC_EXTRACT_DIR"

echo "\$@ is $@"

# debian busybox calls it's own applet by default, so this will call busybox version of ls
# see https://unix.stackexchange.com/questions/274273/are-busybox-commands-truly-built-in
ls --help
