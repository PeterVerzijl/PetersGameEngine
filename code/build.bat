@echo off

set CommonCompilerFlags= -MTd -nologo -fp:fast -Gm- -GR- -EHa- -Oi -WX -W4 -wd4201 -wd4100 -wd4505 -wd4189 -DINTERNAL=1 -DSLOW=1 -FC -Z7
set CommonLinkerFlags= -incremental:no -opt:ref User32.lib Gdi32.lib Winmm.lib

call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64
IF NOT EXIST ..\build mkdir ..\build
pushd ..\build
rem turn on -wd.... once in a while, since they are not referenced parameters and local varialbes
rem -Oi Cpu intrinsics // -GR- runtime information // -EHa- turn off exceptions
rem -MT please link in and package the multithreaded static library into our exe
rem -Gm- Stop incremental builds // -Fmwin32_layer.map creates a map file with all locations of functions in your exe
rem -opt:ref don't put stuff in my exe I don't need.

rem 32-bit build
rem cl %CommonLinkerFlags% ..\code\win32_layer.cpp /link %CommonLinkerFlags% -subsystem:windows,5.1

rem 64-bit build
del *.pdb > NUL 2> NUL
cl %CommonCompilerFlags% ..\code\game.cpp -Fmgame.map -LD /link -incremental:no -opt:ref -PDB:game_%random%.pdb -EXPORT:GameUpdateAndRender -EXPORT:GameGetSoundSamples
cl %CommonCompilerFlags% ..\code\win32_layer.cpp -Fmwin32_layer.map /link %CommonLinkerFlags%

popd