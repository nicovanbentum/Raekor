# Raekor Renderer

![image](https://i.imgur.com/iv3ytur.jpg)

# Build

### Windows
This project relies on [VCPKG](https://github.com/microsoft/vcpkg) to build and link the big binaries, make sure it is installed correctly and integrated user-wide.

Clone this repository using
```git clone --recursive https://github.com/nicovanbentum/Raekor.git ``` to also pull in all the submodules.

Next make sure you have the Vulkan SDK installed, you can get the latest version [here](https://vulkan.lunarg.com/sdk/home#sdk/downloadConfirm/latest/windows/vulkan-sdk.exe).

![image](https://i.imgur.com/2VFTJFH.png)

Copy over ```init.bat``` from Raekor to VCPKG's directory, run it there and let it do its thing (can take a while to compile Assimp and Bullet3). Once that is done open up the supplied Visual Studio solution (re-target to newest if necessary). ```CTRL+B``` to build, ```CTRL+F5``` to run.

### Linux
Used to work through the supplied makefile, but it's out of date.

# Features
This project requires and uses at least C++ 17 and modern rendering API's, OpenGL requires version 4.5 for direct state access functions and shader extensions. Focus has currently shifted to getting features in fast in OpenGL so I can use the engine to  research and implement Global Illumination for a research course at the Hogeschool Utrecht. After finishing my research course I will be migrating the engine to Vulkan (not sure if I will keep OpenGL support) and start working on physically based shading.

✅ Shader hotloading\
✅ Directional & Point lighting\
✅ Shadow mapping (point & directional)\
✅ Normal mapping\
✅ HDR, Gamma & Tone mapping\
✅ Deferred render pipeline\
⬜️ PBR Material system\
✅ C++ 17 Task-based Concurrency\
✅ Screen Space Ambient Occlusion\
⬜️ (Screen Space) Reflections\
✅ Voxel Cone Traced Global Illumination\
⬜️ Scripting language\
✅ Data driven Entity Component System

> More to follow.
