#!busybox sh
#!sh
# both works

echo "\$@ is $@"

# debian busybox calls it's own applet by default
# see https://unix.stackexchange.com/questions/274273/are-busybox-commands-truly-built-in
ls --help

# remove extract dir /tmp/ssc/XXXXXX, this will delete all extracted files
rm -rf "$SSC_EXTRACT_DIR"
