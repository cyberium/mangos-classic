@ECHO off
ECHO Building project with default setting
cmake -H. -Bbin/builddir

ECHO Building binaries in bin folder
cmake --build bin/builddir --config Release