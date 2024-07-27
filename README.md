# Build (CMake - Windows)

### DirectX 12 - Requirements
* Graphics card with RTX and Shader Model 6_6 support.
* Windows SDK 10.0.20348 or higher (configure using the Visual Studio Installer).

## Instructions

Clone this repository using
 ```git clone --recursive https://github.com/nicovanbentum/Raekor.git```
 >**_NOTE:_** If the path to the repository contains any whitespace the shaders will fail to compile.
 
* Run ``` cmake -B Build ``` from the root of the repository. This can take a while as it builds everything, there are no pre-built binaries.

* Build using the generated ```Solution.sln``` visual studio solution inside of the newly created ```Build``` folder.

# Projects

### Engine _(requires C++20)_

* Simple Job System
* Simple CVar system
* JoltPhysics Integration
* Hotloadable C++ scripts (WIP)
* Skinning & Animation
* Custom Scene Format
    - GLTF / FBX / OBJ import (cgltf, ufbx, assimp)
    - RTTI-based Serialisation
    - Entity Component System
    - Multi-threaded Asset Loading

### Renderer

![image](https://svgshare.com/i/yZn.svg)

Basic RenderGraph architecture (automatically creates resource views and handles resource barriers) on top of DirectX 12. Heavily relies on shader model 6_6 bindless. Lots of ray tracing. Implements an editor, so can edit the scene in real-time. The rendergraph currently features:

- Compute Skinning Pass
- Geometry Buffer Pass
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

Implements a scene editor using ImGui that can transform objects, adjust materials, merge scenes, create sequences and more.

![image](https://i.imgur.com/7haNfzV.png)
