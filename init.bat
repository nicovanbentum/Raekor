@echo off

vcpkg install assimp --triplet x64-windows
vcpkg install bullet3 --triplet x64-windows
vcpkg install sdl2 --triplet x64-windows
xcopy /s "installed\x64-windows\include\SDL2" "installed\x64-windows\include" /Y > nul
vcpkg remove sdl --triplet x64-windows
vcpkg remove sdl1 --triplet x64-windows
vcpkg install directxtk --triplet x64-windows
vcpkg install lz4 --triplet x64-windows

echo.
echo Initialization finished.
echo.
pause