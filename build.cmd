@echo off
setlocal
cd /d "%~dp0"
zig cc -target x86_64-windows-gnu -shared -O2 -Wall -Wextra -Werror DarkMode.c -o "..\DarkMode.dll" -lcomctl32 -ldwmapi -lgdi32 -luser32
if errorlevel 1 exit /b 1
zig cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror smoke-test.c -o "..\smoke-test.exe" -lcomctl32 -lgdi32 -luser32
