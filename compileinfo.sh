#!/bin/bash

echo '// Automatically generated file. See project file and compileinfo.sh for further information.'
date --utc '+const char *pCompileInfoTimestampBuild     = "%Y-%m-%d-%H.%M.%S UTC";'
date --utc '+const char *pCompileInfoTimestampChangelog = "%Y-%m-%d-%H.%M.%S UTC";' -d "$(dpkg-parsechangelog -S Date)"

# Line below: changelog figures twice because subdirectory debian is missing in Sourceforge upload 
# (changelog is in source code directory there). By putting it twice, it works in any case.
head -qn 1 changelog debian/changelog 2>/dev/null | awk '{
                                    Version = $2
                                    gsub ("\\(", "", Version)
                                    gsub ("\\)", "", Version)
                                    print "const char *pCompileInfoVersion   = \"" Version "\";"}'

