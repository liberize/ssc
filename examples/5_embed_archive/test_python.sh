#! PYTHONHOME=$SSC_EXTRACT_DIR/python ./python/bin/python3
# use environment variables just like in shell

import os
import sys
import shutil

print(os.environ)
print(sys.argv)

# remove extract dir /tmp/ssc/XXXXXX, this will delete all extracted files
shutil.rmtree(os.environ["SSC_EXTRACT_DIR"])
