@echo off

mkdir ..\build
pushd ..\build
cl  -FC -Zi ..\code\main.cpp /link User32.lib Gdi32.lib
popd