@echo off
REM windows build, ms x64 abi, needs nasm and mingw-w64 gcc on your PATH
nasm -f win64 imgcvt.asm -o imgcvt.obj
if errorlevel 1 goto fail
gcc main.c imgcvt.obj -o mp2.exe
if errorlevel 1 goto fail
echo built mp2.exe, run it with mp2.exe
goto end
:fail
echo build failed
:end
