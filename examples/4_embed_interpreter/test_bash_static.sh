#!bash-static

echo "\$@ is $@"
echo "hello world"

# remove extract dir /tmp/ssc/XXXXXX, this will delete all extracted files
rm -rf "$SSC_EXTRACT_DIR"
