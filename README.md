# LunaTextractor dark mode

A native Windows x64 UI plugin for LunaHost v3.6.0, plus a Qt plugin stylesheet.

## Fix for the white hook list

Version 1.0.1 keeps the list background and text colors dark when the host resets
them after initialization, and restores the palette before painting after a theme
change. It uses native list-view color messages; list selection, scrolling and
text extraction remain handled by the original application.

To update the previous dark-mode package: close LunaHost, replace `DarkMode.dll`
with the DLL in the release ZIP, and restart. No configuration changes are needed.
The patch is for the 64-bit GUI only. The source archive's 32-bit hook DLLs are
unrelated and are not replaced.

## Validation

GitHub Actions runs the Windows regression harness against both the old DLL
(expected assertion failure) and the shipping DLL (expected pass). The harness
checks palette resets, theme changes, dynamically created windows, checkboxes,
Japanese text preservation and safe plugin module lifetime.

The runner is Windows Server 2022. It is not an interactive Windows 10/11 game
attachment test. Native scrollbars and OS dialogs retain system styling.

## Build

With Zig installed, from this directory:

```
zig cc -target x86_64-windows-gnu -shared -O2 -Wall -Wextra -Werror DarkMode.c -o bin/DarkMode.dll -lcomctl32 -ldwmapi -lgdi32 -luser32
```

The repository contains only the theme source, plugin and test fixtures. It does
not contain personal game paths, saved hooks, or the original third-party app.
