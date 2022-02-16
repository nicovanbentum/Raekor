# Build (CMake - Windows)
* To build the Raekor project <ins>**any**</ins> Windows 10 SDK needs to be installed, to build the DX12 project The _**latest**_ Windows 10 SDK needs to be installed. Configure either using the Visual Studio Installer.
 >**_NOTE:_** Last passing version was ```10.0.22```

* To build the Vulkan project the _**latest**_ Vulkan SDK needs to be installed, get it [here](https://vulkan.lunarg.com/sdk/home#sdk/downloadConfirm/latest/windows/vulkan-sdk.exe).
 >**_NOTE:_** Last passing version was ```1.3.204.0```

Clone this repository using
 ```git clone --recursive https://github.com/nicovanbentum/Raekor.git```
 >**_NOTE:_** If the path to the repository contains any whitespace the shaders will fail to compile.
 
* Run ``` cmake . ``` from the root of the repository. This can take a while as it builds everything, there are no pre-built binaries.

* Build using the generated ```Solution.sln``` visual studio solution.

# Projects

### Raekor
This project requires C++ 17.

* Simple Job system
* Simple ConVar system
* PhysX Integration (WIP)
* Hotloadable C++ Scripts (WIP)
* Custom Scene Format
    - DCC import (Assimp)
    - Serialisation (Cereal)
    - Entity component system (EnTT)
    - S3TC DXT5 texture compression


### Editor
This project requires OpenGL 4.6 for direct state access functions and shader include directives. 

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

![image](https://i.imgur.com/2PCUuBm.png)
*Crytek's Sponza with hard ray traced shadows using Vulkan and voxel cone traced first bounce global illumination.*

![image2](https://i.imgur.com/htxWnRu.png)
*Blender's splash screen scene.*

### VK
This project requires Vulkan 1.2 with support for descriptor indexing, device buffer address, and hardware ray tracing. It is currently being rewritten to implement a GPU path tracer using the vk_khr_ray_tracing extensions. It used to contain experiments with parallel command buffer recording, dynamic uniform buffers, and bindless textures. Also used to integrate [Scatter](https://github.com/nicovanbentum/Scatter) in a separate branch.

![image](https://i.imgur.com/LgjcfKD.png)
*Path tracer WIP - Very basic diffuse, 8 bounces*

### DX12
*Soon(TM)*

### DX11
The project started out exploring switching between graphics APIs at runtime, which is why you will find a bunch of unfinished DirectX 11. Source only.

### Ray Tracing in One Weekend
Implements Ray Tracing in One Weekend by Peter Shirley in a GLSL Compute shader that accumulates over time. Source only.
