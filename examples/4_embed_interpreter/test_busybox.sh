#!busybox sh
#!sh
# both works

echo "\$@ is $@"

# the interpreter directory is already prepended to PATH, just call busybox
# if you want to call busybox applet without ./busybox
# you have to recompile it with FEATURE_SH_STANDALONE and FEATURE_PREFER_APPLETS
# see https://unix.stackexchange.com/questions/274273/are-busybox-commands-truly-built-in
busybox ls -l
