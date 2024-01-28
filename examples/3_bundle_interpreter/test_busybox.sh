#!./busybox sh

echo "\$@ is $@"
# SSC_EXE_PATH is a builtin variable
echo "current executable path is $SSC_EXE_PATH"

# we need to cd to busybox directory to call busybox
cd "$(dirname "$SSC_EXE_PATH")"

# if you want to call busybox applet without ./busybox
# you have to recompile it with FEATURE_SH_STANDALONE and FEATURE_PREFER_APPLETS
# see https://unix.stackexchange.com/questions/274273/are-busybox-commands-truly-built-in
./busybox ls -l
