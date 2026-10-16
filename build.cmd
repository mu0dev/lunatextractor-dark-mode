@echo off
setlocal
cd /d "%~dp0"
zig cc -target x86_64-windows-gnu -shared -O2 -Wall -Wextra -Werror DarkMode.c -o "bin\DarkMode.dll" -lcomctl32 -ldwmapi -lgdi32 -luser32
if errorlevel 1 exit /b 1
zig cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror tests\smoke-test.c -o "bin\smoke-test.exe" -lcomctl32 -lgdi32 -luser32
if errorlevel 1 exit /b 1
copy /y "tests\smoke-test.manifest" "bin\smoke-test.exe.manifest" >nul
