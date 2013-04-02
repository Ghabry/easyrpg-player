@echo off
:: If git repository:
::  Writes current git revision in src/revision.h
:: If not a git repository (e.g. source tarball):
::  revision.h exists: Do nothing
::  revision.h missing: Write version "Unknown Version" in src/revision.h

pushd %~dp0\..

IF NOT EXIST .git (
    IF EXIST src/revision.h (
        goto end
    )    
    SET GITREV=#define PLAYER_REVISION "Unknown Version"
) ELSE (
    for /F "tokens=*" %%a in ('git describe --abbrev^=4 --long') do (
        SET GITREV=#define PLAYER_REVISION "%%a"
    )
)

cd /d src

echo #ifndef EASYRPG_PLAYER_REVISION_H > revision.h
echo #define EASYRPG_PLAYER_REVISION_H >> revision.h
echo %GITREV% >> revision.h
echo #endif >> revision.h

:end
popd
