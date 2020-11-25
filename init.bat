@echo off

vcpkg install assimp --triplet x64-windows-static
vcpkg install bullet3 --triplet x64-windows-static
vcpkg install sdl2[vulkan] --triplet x64-windows-static
vcpkg install lz4 --triplet x64-windows-static
vcpkg install spirv-cross --triplet x64-windows-static

python -m dependencies/glad/glad --generator=c --out-path=GL --profile=core --api=gl=4.6

echo.
echo [1m[32mPackage install finished.[0m[0m
echo.
pause