@echo off

call "c:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64
SUBST G: /D
subst G: C:\Users\User\Documents\GitHub\PetersGameEngine
G:
cd \code
