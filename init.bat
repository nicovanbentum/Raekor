@echo off

vcpkg install assimp --triplet x64-windows-static
vcpkg install bullet3 --triplet x64-windows-static
vcpkg install sdl2[vulkan] --triplet x64-windows-static
vcpkg install lz4 --triplet x64-windows-static
vcpkg install spirv-cross --triplet x64-windows-static

echo.
echo [1m[32mPackage install finished.[0m[0m
echo.
pause