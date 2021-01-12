@echo off

python -m glad/glad --generator=c --out-path=GL --profile=core --api=gl=4.6

vcpkg/bootstrap-vcpkg.bat

vcpkg/vcpkg install lz4:x64-windows-static
vcpkg/vcpkg install assimp:x64-windows-static
vcpkg/vcpkg install physx:x64-windows-static
vcpkg/vcpkg install sdl2[vulkan]:x64-windows-static
vcpkg/vcpkg install spirv-cross:x64-windows-static

vcpkg/vcpkg integrate project

