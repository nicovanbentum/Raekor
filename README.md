# Build (CMake - Windows)

### Editor - Requirements
* Graphics card with OpenGL 4.5 support.

### Vulkan - Requirements
* Graphics card with RTX support.
* Vulkan SDK [1.3.204](https://sdk.lunarg.com/sdk/download/1.3.204.0/windows/VulkanSDK-1.3.204.0-Installer.exe) or higher.

### DirectX 12 - Requirements
* Graphics card with RTX and Shader Model 6_6 support.
* Windows SDK 10.0.20348 or higher (configure using the Visual Studio Installer).

## Instructions

Clone this repository using
 ```git clone --recursive https://github.com/nicovanbentum/Raekor.git```
 >**_NOTE:_** If the path to the repository contains any whitespace the shaders will fail to compile.
 
* Run ``` cmake -B build ``` from the root of the repository. This can take a while as it builds everything, there are no pre-built binaries.

* Build using the generated ```Solution.sln``` visual studio solution inside of the newly created ```build``` folder.

# Projects

### Raekor
This project requires C++ 20.

* Simple Job System
* Simple CVar system
* JoltPhysics Integration
* Hotloadable C++ scripts (WIP)
* Compute based Skinning & Animation (OpenGL & DX12 Only)
* Custom Scene Format
    - GLTF / FBX / OBJ import (cgltf, ufbx, assimp)
    - RTTI-based Serialisation
    - Entity Component System
    - Multi-threaded Asset Loading

### DX12
*Main project at the moment.*

![image](https://svgshare.com/i/yZn.svg)

Basic RenderGraph architecture (automatically creates resource views and handles resource barriers) on top of DirectX 12. Heavily relies on shader model 6_6 bindless. Lots of ray tracing. Implements IEditor, so can edit the scene in real-time. The rendergraph currently features:

- Skinning Pass
- GBuffer Pass
- Path Trace Pass (For comparisons)
- Ray Traced Shadows (denoiser WIP)
- Ray Traced Ambient Occlusion (denoiser WIP)
- Ray Traced Reflections (denoiser WIP)
- Ray Traced Irradiance Probes (DDGI)
- Deferred Shading
- TAA / FSR2 / DLSS / XeSS
- Post Processing

![image](https://i.imgur.com/B3pbNgd.png)

### Editor
This project requires OpenGL 4.5 for direct state access functions.

* Deferred PBR Renderer
    - Bloom
    - ACES Tonemapping
    - Cook-Torrance BRDF
    - Temporal Anti-Aliasing
    - Cascaded Shadow Mapping
    - Voxel Cone Traced Ambient Occlusion
    - Voxel Cone Traced Specular Reflections
    - Voxel Cone Traced Global Illumination (single bounce diffuse)
    - Ray Traced Hard Shadows using Vulkan interop (uses the **DEPRECATED** vk_nv_ray_tracing extension from [Scatter](https://github.com/nicovanbentum/Scatter))

![image](https://i.imgur.com/m8HLdED.png)

### VK
Requires Vulkan 1.2 with support for descriptor indexing, device buffer address, and hardware ray tracing. 
* Uni-Directional GPU Path Tracer using the vk_khr_ray_tracing extensions.
   - Progressive real-time rendering
   - Importance sampled diffuse and specular BRDF
   
Older versions of this project contained rasterized experiments with parallel command buffer recording, dynamic uniform buffers, and bindless textures.

![image](https://i.imgur.com/S4l11hb.jpg)
*Path tracer WIP - 8 bounces. Converges in real time when stationary.*

### DX11
A small demo of async GPU resource creation and dithered mesh fading.
Raekor started out 4 years ago as an experiment on switching between graphics APIs at runtime (I had no idea what I was doing lol), which is why you will find a bunch of unfinished DirectX 11. Source only.

### Ray Tracing in One Weekend
![image](https://i.imgur.com/7haNfzV.png)
Single school assignment, implements Ray Tracing in One Weekend by Peter Shirley in a GLSL Compute shader. Source only.
