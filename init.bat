@echo off

vcpkg install assimp --triplet x64-windows
vcpkg install bullet3 --triplet x64-windows
vcpkg install sdl2[vulkan] --triplet x64-windows
vcpkg install directxtk --triplet x64-windows
vcpkg install lz4 --triplet x64-windows

echo.
echo [1m[32mPackage install finished.[0m[0m
echo.
pause