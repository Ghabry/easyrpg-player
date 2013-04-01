#!/bin/bash

SCRIPT_DIR="`dirname \"$0\"`"
GITREV=`git describe --abbrev=4 --long`

cd $SCRIPT_DIR/../src

echo "#ifndef EASYRPG_PLAYER_REVISION_H" > revision.h
echo "#define EASYRPG_PLAYER_REVISION_H" >> revision.h
echo "#define PLAYER_REVISION \"$GITREV\"" >> revision.h
echo "#endif" >> revision.h
