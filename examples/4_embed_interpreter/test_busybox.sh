#!busybox sh
#!sh
# both works

echo "\$@ is $@"

# debian busybox calls it's own applet by default
# see https://unix.stackexchange.com/questions/274273/are-busybox-commands-truly-built-in
ls --help
