@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64
IF NOT EXIST ..\build mkdir ..\build
pushd ..\build
rem turn on -wd4100 -wd4189 once in a while, since they are not referenced parameters and local varialbes
rem -Oi Cpu intrinsics // -GR- runtime information // -EHa- turn off exceptions
rem -MT please link in and package the multithreaded static library into our exe
rem -Gm- Stop incremental builds // -Fmwin32_layer.map creates a map file with all locations of functions in your exe
rem -opt:ref don't put stuff in my exe I don't need.
rem -subsystem:windows,5.1
cl -MT -nologo -Gm- -GR- -EHa- -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -DINTERNAL=1 -DSLOW=1 -FC -Z7 -Fmwin32_layer.map ..\code\win32_layer.cpp /link -subsystem:windows,5.1 -opt:ref User32.lib Gdi32.lib
popd