@echo off

SUBST G: /D
subst G: C:\Users\User\Documents\GitHub\PetersGameEngine
G:
cd \code
call "c:\Program Files (x86)\Visual Studio 14.0\VC\vcvarsall.bat" x64
