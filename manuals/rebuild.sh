#!/bin/bash

TH_Date=`date '+%Y-%m-%d'`
TH_Source=`head -qn 1 ../debian/changelog ../changelog 2>/dev/null | awk '{
                                                  Version = $2
                                                  gsub ("\\\\(", "", Version)
                                                  gsub ("\\\\)", "", Version)
                                                  print Version}'`

echo .TH guymager 1  "\"$TH_Date\" \"version $TH_Source\" \"guymager manual pages\"" > guymager.1
cat guymager_body.1 >> guymager.1

