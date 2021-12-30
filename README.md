# Raekor Renderer

![image](https://i.imgur.com/2PCUuBm.png)
*Crytek's Sponza with hard ray traced shadows using Vulkan and voxel cone traced first bounce global illumination.*

![image2](https://i.imgur.com/htxWnRu.png)
*Blender's splash screen scene.*

![image3](https://i.imgur.com/EVhb0gh.png)
*Dancing stormtrooper.*

# Build (CMake - Windows)

* The _**latest**_ Vulkan SDK needs to be installed, get it [here](https://vulkan.lunarg.com/sdk/home#sdk/downloadConfirm/latest/windows/vulkan-sdk.exe).
 >**_NOTE:_** Last passing version was ```1.2.182.0```

* Python 3+ needs to be installed, get it [here](https://www.python.org/downloads/).

* Clone this repository using
 ```git clone --recursive https://github.com/nicovanbentum/Raekor.git```
 >**_NOTE:_** If the path to the repository contains any whitespace the shaders will fail to compile.
 
* Run ``` cmake . ``` from the root of the repository. This can take a while as it builds all the libraries.

* Build using the generated ```Raekor.sln``` visual studio solution.

>**_NOTE:_** Using the ```-gl``` or ```-vk``` command line arguments you can switch between the OpenGL editor and the Vulkan path tracer, defaults to the editor. The repository provides a json file that works with the [SwitchStartupProject](https://marketplace.visualstudio.com/items?itemName=vs-publisher-141975.SwitchStartupProject) Visual Studio extension.

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
    - DCC import (Assimp)
    - Serialisation (Cereal)
    - Entity component system (EnTT)
    - S3TC DXT5 texture compression

### Vulkan
The Vulkan side of things is currently being rewritten to implement a GPU path tracer using the vk_khr_ray_tracing extensions. It used to contain experiments with parallel command buffer recording, dynamic uniform buffers, and bindless textures. Also used to integrate [Scatter](https://github.com/nicovanbentum/Scatter) in a separate branch.

![image](https://i.imgur.com/LgjcfKD.png)
*Path tracer WIP - Very basic diffuse, 8 bounces*

### Ray Tracing in One Weekend
There's a third entry point that implements Ray Tracing in One Weekend by Peter Shirley in a GLSL Compute shader that accumulates over time. 
