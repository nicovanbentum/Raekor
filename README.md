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
Used to work through the supplied makefile, but it's out of date. I will not be working on Linux support for the forseeable future.

# Features
This project requires and uses at least C++ 17 and modern rendering API's, OpenGL requires at least version 4.2 for direct state access functions and shader extensions. This project started out by exploring possible abstractions of multiple render API's which is why you'll find a bunch of unfinished DirectX11 and Vulkan code. Currently, the renderer only supports OpenGL. Some of the features currently implemented are shader hotloading, Blinn-Phong point and directional lighting, shadow mapping for both, normal mapping, HDR and tonemapping, a deferred rendering pipeline, screen-space ambient occlusion, voxel cone traced indirect diffuse light and specular reflections, ChaiScript scripting language, a data-driven entity component system and a simple job system.
