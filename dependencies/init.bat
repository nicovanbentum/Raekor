@echo off

cd glad
python -m glad --generator=c --out-path=GL --profile=core --api=gl=4.6
cd ../

call "vcpkg/bootstrap-vcpkg.bat"

"vcpkg/vcpkg.exe" install lz4:x64-windows-static
"vcpkg/vcpkg.exe" install assimp:x64-windows-static
"vcpkg/vcpkg.exe" install physx:x64-windows-static
"vcpkg/vcpkg.exe" install sdl2[vulkan]:x64-windows-static
"vcpkg/vcpkg.exe" install spirv-cross:x64-windows-static

"vcpkg/vcpkg.exe" integrate project

PAUSE



