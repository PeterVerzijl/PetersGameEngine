@echo off
cd "..\"
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64
subst G: /D
subst G: "%CD%\PetersGameEngine"
G:
cd ".\code\"