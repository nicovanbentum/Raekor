# Raekor Renderer

![image](https://i.imgur.com/2PCUuBm.png)
*Crytek's Sponza with hard ray traced shadows using Vulkan and voxel cone traced first bounce global illumination.*

![image2](https://i.imgur.com/htxWnRu.png)
*Blender's splash screen scene.*

# Build

### Windows
This project relies on [VCPKG](https://github.com/microsoft/vcpkg) to build and link the big binaries. Make sure it is installed correctly and integrated user wide.

Next make sure you have the _**latest**_ Vulkan SDK installed, you can get it [here](https://vulkan.lunarg.com/sdk/home#sdk/downloadConfirm/latest/windows/vulkan-sdk.exe).

Clone this repository using
 ```git clone --recursive https://github.com/nicovanbentum/Raekor.git```  to also pull in all the submodules.

Build using the supplied Visual Studio solution (re-target to newest if necessary).

### Linux
Used to work through the supplied makefile, but it's out of date. No Linux support for the forseeable future.

# Features
This project requires C++ 17 and OpenGL 4.6 for direct state access functions and shader include directives. The project started out exploring switching between graphics APIs at runtime, which is why you will find a bunch of unfinished DirectX 11.

### Editor

* Simple Job system
* Simple ConVar system
* PhysX Integration (WIP)
* Hotloadable C++ Scripts (WIP)
* Compute based Skinning & Animation
* Deferred PBR Renderer
    - Bloom
    - ACES Tonemapping
    - Cook-Torrance BRDF
    - Accurate Atmospheric Sky
    - Cascaded Shadow Mapping
    - Voxel Cone Traced Ambient Occlusion
    - Voxel Cone Traced Specular Reflections
    - Voxel Cone Traced Global Illumination (single bounce diffuse)
* Scenes
    - DDC import (Assimp)
    - Serialisation (Cereal)
    - Entity component system (EnTT)
    - S3TC DXT5 texture compression

### Vulkan
The Vulkan side of things is currently being rewritten to implement a GPU path tracer using the new vk_khr_ray_tracing extensions. It used to contain experiments with parallel command buffer recording, dynamic uniform buffers, and bindless textures. Also used to integrate [Scatter](https://github.com/nicovanbentum/Scatter) in a separate branch.

### Ray Tracing in One Weekend
There's a third entry point that implements Ray Tracing in One Weekend by Peter Shirley in a GLSL Compute shader that accumulates over time. 
