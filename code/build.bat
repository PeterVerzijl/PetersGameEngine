@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64
IF NOT EXIST ..\build mkdir ..\build
pushd ..\build
ren -WX -wd4201 -W4 -DHANDMADE_WIN32=1
cl -DINTERNAL=1 -DSLOW=1 -FC -Zi ..\code\win32_layer.cpp /link User32.lib Gdi32.lib
popd