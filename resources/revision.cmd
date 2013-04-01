@echo off
:: Writes current git revision in src/revision.h

pushd %~dp0\..

for /F "tokens=*" %%a in ('git describe --abbrev^=4 --long') do (
    SET GITREV=#define PLAYER_REVISION "%%a"
)

cd /d src

echo #ifndef EASYRPG_PLAYER_REVISION_H > revision.h
echo #define EASYRPG_PLAYER_REVISION_H >> revision.h
echo %GITREV% >> revision.h
echo #endif >> revision.h

popd
