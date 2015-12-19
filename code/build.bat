@echo off

mkdir ..\..\build
pushd ..\..\build
cl ..\code\main.cpp /Zi /link User32.lib Gdi32.lib
popd