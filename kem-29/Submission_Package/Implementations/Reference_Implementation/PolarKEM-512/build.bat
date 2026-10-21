@echo off
setlocal

if /I "%~1"=="clean" goto clean

where gcc >nul 2>nul
if errorlevel 1 (
    echo ERROR: gcc was not found on PATH. 1>&2
    exit /b 1
)

set "COMMON_FLAGS=-std=c99 -Wall -Wextra -Wpedantic -Werror"
if /I "%~1"=="sanitize" goto sanitizer

gcc %COMMON_FLAGS% -O2 KEM_AlgorithmInstance.c KAT_KEM.c auxfunc.c drng.c polarkem_core.c polarkem_polar.c polarkem_pack.c polarkem_ct.c -o KAT_KEM.exe
if errorlevel 1 exit /b 1
exit /b 0

:sanitizer
call "%~f0" clean
rem Instrument KAT and project code fully.  For the exact official helpers,
rem disable only UBSan's known shift check in the upstream SM3 rotate macro.
gcc %COMMON_FLAGS% -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -c KEM_AlgorithmInstance.c polarkem_core.c polarkem_polar.c polarkem_pack.c polarkem_ct.c KAT_KEM.c
if errorlevel 1 exit /b 1
gcc %COMMON_FLAGS% -O1 -g -fsanitize=address,undefined -fno-sanitize=shift -fno-omit-frame-pointer -c auxfunc.c drng.c
if errorlevel 1 exit /b 1
gcc -fsanitize=address,undefined KEM_AlgorithmInstance.o polarkem_core.o polarkem_polar.o polarkem_pack.o polarkem_ct.o KAT_KEM.o auxfunc.o drng.o -o KAT_KEM.exe
if errorlevel 1 exit /b 1
exit /b 0

:clean
del /q KAT_KEM.exe *.o *.d 2>nul
exit /b 0
