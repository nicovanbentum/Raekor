# Build (CMake - Windows)

### Raekor - Static Library
* Any Windows 10 SDK (configure using the Visual Studio Installer).

### Editor - OpenGL Application
* Requires Raekor.
* Graphics card with OpenGL 4.6 support.
* Vulkan SDK accessible from PATH.

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

* Simple Job System
* Simple CVar system
* JoltPhysics Integration
* Hotloadable C++ scripts (WIP)
* Custom Scene Format
    - GLTF / FBX / OBJ import (cgltf, ufbx, assimp)
    - RTTI-based Serialisation
    - Entity Component System
    - S3TC DXT5 Texture conversion


### Editor
This project requires OpenGL 4.6 for direct state access functions and shader include directives. 

* Compute based Skinning & Animation
* Deferred PBR Renderer
    - Bloom
    - ACES Tonemapping
    - Cook-Torrance BRDF
    - Temporal Anti-Aliasing
    - Cascaded Shadow Mapping
    - Voxel Cone Traced Ambient Occlusion
    - Voxel Cone Traced Specular Reflections
    - Voxel Cone Traced Global Illumination (single bounce diffuse)
    - Ray Traced Hard Shadows using Vulkan interop (only availabe in the [Scatter](https://github.com/nicovanbentum/Scatter)-integration branch, very outdated using the vk_nv_ray_tracing extension).
* Scenes
    - Simulate basic physics shapes.
    - Import models from GLTF, FBX and OBJ file formats.
    - Edit, add, delete, duplicate, and transform entities/components.

![image](https://i.imgur.com/m8HLdED.png)

### VK
Requires Vulkan 1.2 with support for descriptor indexing, device buffer address, and hardware ray tracing. 
* Uni-Directional GPU Path Tracer using the vk_khr_ray_tracing extensions.
   - Progressive real-time rendering
   - Importance sampled diffuse and specular BRDF
   
Older versions of this project contained rasterized experiments with parallel command buffer recording, dynamic uniform buffers, and bindless textures.

![image](https://i.imgur.com/S4l11hb.jpg)
*Path tracer WIP - 8 bounces. Converges in real time when stationary.*

### DX12
*Currently In-Progress*

Basic RenderGraph architecture (automatically creates resource views and handles resource transitions) on top of DirectX 12. Heavily relies on shader model 6_6 bindless. Lots of ray tracing. Derived from Editor, so can edit the scene in real-time. The rendergraph currently features:

- GBuffer Pass
- Path Trace Pass (For comparisons)
- Procedural Grass (based on Ghost of Tsushima, WIP)
- Ray Traced Shadows (denoiser WIP)
- Ray Traced Ambient Occlusion (denoiser WIP)
- Ray Traced Reflections (denoiser WIP)
- Ray Traced Irradiance Probes (DDGI)
- Deferred Shading
- TAA / FSR2 / DLSS / XeSS (Upscalers are WIP)
- Post Processing

![image](https://svgshare.com/i/yXg.svg)
![image](https://i.imgur.com/B3pbNgd.png)

### DX11
Currently a small demo of async GPU resource creation and dithered mesh fading.
Raekor started out 4 years ago as an experiment on switching between graphics APIs at runtime (I had no idea what I was doing lol), which is why you will find a bunch of unfinished DirectX 11. Source only.

### Ray Tracing in One Weekend
![image](https://i.imgur.com/7haNfzV.png)
Single school assignment, implements Ray Tracing in One Weekend by Peter Shirley in a GLSL Compute shader. Source only.
