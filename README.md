# Raekor Renderer

![image](https://i.imgur.com/2PCUuBm.png)
*Crytek's Sponza with hard ray traced shadows using Vulkan.*

# Build

### Windows
This project relies on [VCPKG](https://github.com/microsoft/vcpkg) to build and link the big binaries. Make sure it is installed correctly, integrated user wide, and added to PATH by registering the Powershell tools (```vcpkg integrate powershell```).

Clone this repository using
```git clone --recursive https://github.com/nicovanbentum/Raekor.git ``` to also pull in all the submodules.

Next make sure you have the _**latest**_ Vulkan SDK installed, you can get it [here](https://vulkan.lunarg.com/sdk/home#sdk/downloadConfirm/latest/windows/vulkan-sdk.exe).

Double click ```init.bat``` and let it do its thing (can take a while to compile Assimp and Bullet3). Once that is done open up the supplied Visual Studio solution (re-target to newest if necessary). ```CTRL+B``` to build, ```CTRL+F5``` to run.

### Linux
Used to work through the supplied makefile, but it's out of date. No Linux support for the forseeable future.

# Features
This project requires C++ 17 and OpenGL 4.5 for direct state access functions and shader extensions. The project started out exploring possible abstractions for multiple render APIs, which is why you will find a bunch of unfinished DirectX 11 and Vulkan code. Currently, the editor only supports OpenGL. Some of the features implemented are shader hotloading, Blinn-Phong point and directional lighting, shadow mapping for both, normal mapping, HDR and tonemapping, a deferred rendering pipeline, screen-space ambient occlusion, voxel cone traced first bounce diffuse light and specular reflections, a data-driven entity component system and a simple job system. The Editor allows users to import meshes, materials and animations from GLTF and FBX files and play around with scene composition. At any time the user can save the current scene to disk in a compressed binary format. This format is much quicker to serialize compared to Blender files.

# Future
Even though there are tons of graphical techniques I want to implement I have shifted focus to learning and implementing Vulkan. I have decided to shift focus to implementing physically based rendering and potentially ray traced shadows and reflections through the vk_khr_ray_tracing Vulkan extension. At some point in the future I would also like to learn more about game engine development and add physics (Bullet3) and scripting (Mono).
