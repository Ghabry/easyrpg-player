#!/bin/bash
# If git repository:
#  Writes current git revision in src/revision.h
# If not a git repository (e.g. source tarball):
#  revision.h exists: Do nothing
#  revision.h missing: Write version "Unknown Version" in src/revision.h

SCRIPT_DIR="`dirname \"$0\"`"

cd $SCRIPT_DIR/..

if [ -d ".git" ]
then
    GITREV=`git describe --abbrev=4 --long`
else
    if [ -e src/revision.h ]
    then
        exit
    fi
    GITREV="Unknown Version"
fi

cd $SCRIPT_DIR/../src

echo "#ifndef EASYRPG_PLAYER_REVISION_H" > revision.h
echo "#define EASYRPG_PLAYER_REVISION_H" >> revision.h
echo "#define PLAYER_REVISION \"$GITREV\"" >> revision.h
echo "#endif" >> revision.h
