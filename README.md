# Raekor Renderer

![image](https://i.imgur.com/2PCUuBm.png)
*Crytek's Sponza with hard ray traced shadows using Vulkan and voxel cone traced first bounce global illumination.*

# Build

### Windows
This project relies on [VCPKG](https://github.com/microsoft/vcpkg) to build and link the big binaries. Make sure it is installed correctly and integrated user wide.

Next make sure you have the _**latest**_ Vulkan SDK installed, you can get it [here](https://vulkan.lunarg.com/sdk/home#sdk/downloadConfirm/latest/windows/vulkan-sdk.exe).

Clone this repository using
```git clone --recursive https://github.com/nicovanbentum/Raekor.git ``` to also pull in all the submodules.

Build using the supplied Visual Studio solution (re-target to newest if necessary).

### Linux
Used to work through the supplied makefile, but it's out of date. No Linux support for the forseeable future.

# Features
This project requires C++ 17 and OpenGL 4.5 for direct state access functions and shader extensions. The project started out exploring switching between graphics APIs at runtime, which is why you will find a bunch of unfinished DirectX 11 and Vulkan code. Some of the features implemented are PBR directional lights, shadow mapping, normal mapping, 
HDR Tonemapping, Bloom, a deferred rendering pipeline, voxel cone traced first bounce diffuse light and specular reflections with ambient occlusion, a data-driven entity component system and a simple job system. The Editor allows users to import meshes, materials and animations from GLTF and FBX files and play around with scene composition. Scenes can be saved to disk in a compressed binary format that should be fast to load.
