#!busybox sh
#!sh
# both works

# debian busybox calls it's own applet by default, so this will call busybox version of ls
# see https://unix.stackexchange.com/questions/274273/are-busybox-commands-truly-built-in
ls --help 2>&1 | head -n 1
