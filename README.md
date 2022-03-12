# Build (CMake - Windows)

### Raekor - Static Library
* Any Windows 10 SDK (configure using the Visual Studio Installer).

### Editor - OpenGL Application
* Requires Raekor.
* Graphics card with OpenGL 4.6 support.

### VK - Vulkan Application
* Requires Raekor.
* Up-to-date graphics drivers.
* Graphics card with RTX support.
* Vulkan SDK [1.3.204](https://sdk.lunarg.com/sdk/download/1.3.204.0/windows/VulkanSDK-1.3.204.0-Installer.exe) or higher.

### DX - DirectX 12 Application
* Requires Raekor.
* Up-to-date graphics drivers.
* Graphics card with RTX and Shader Model 6_6 support.
* Windows 10 version with Agility SDK support (see Microsoft's [blog post](https://devblogs.microsoft.com/directx/gettingstarted-dx12agility/#OS)).
* Windows SDK 10.0.20348 or higher (configure using the Visual Studio Installer).

## Instructions

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
* Jolt Physics Integration
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
    - Ray Traced Hard Shadows using Vulkan interop (only availabe in the Scatter-integration branch, very outdated using the vk_nv_ray_tracing extension).

![image](https://i.imgur.com/m8HLdED.png)

### VK
This project requires Vulkan 1.2 with support for descriptor indexing, device buffer address, and hardware ray tracing. It is currently being rewritten to implement a GPU path tracer using the vk_khr_ray_tracing extensions. It used to contain experiments with parallel command buffer recording, dynamic uniform buffers, and bindless textures. Also used to integrate [Scatter](https://github.com/nicovanbentum/Scatter) in a separate branch.

![image](https://i.imgur.com/0dYlU8P.jpg)
*Path tracer WIP - 8 bounces. Converges in real time when stationary.*

### DX12
*Coming Soon(TM)*

### DX11
Raekor started out as an experiment on switching between graphics APIs at runtime, which is why you will find a bunch of unfinished DirectX 11. Source only.

### Ray Tracing in One Weekend
![image](https://i.imgur.com/7haNfzV.png)
Single school assignment, implements Ray Tracing in One Weekend by Peter Shirley in a GLSL Compute shader. Source only.
