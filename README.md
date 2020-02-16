# Raekor Engine

![image](https://i.imgur.com/88mkByx.jpg)

# How to build

### Windows
Use the supplied Visual Studio Project, binaries for SDL2 and Assimp are included.

### Linux (BROKEN SINCE VULKAN INTEGRATION)
enter `sudo apt-get install libsdl2-2.0` in a terminal.

Open a terminal and `cd` to `Raekor/dependencies/Assimp/`, do `cmake ./` then `make` then `sudo make install`.

`cd` to the Raekor directory and do `make rebuild`.

Run the application using `make run`.

>*Tested on Ubuntu 19.04*

### Render APIs
There's a basic framework in place for DX11 and VK but focusing on getting features in faster for just OpenGL so I can research Global Illumination for my Research Semester at uni. Some of the features currently being worked on:
- [X] Shader hotloading
- [X] Directional & Point lighting
- [X] Shadow mapping (point & directional)
- [X] Normal mapping
- [X] HDR, Gamma & Tone mapping
- [ ] Deferred render pipeline
- [ ] Physically Based Rendering
- [ ] Screen Space Ambient Occlusion
- [ ] Screen Space Reflections
- [ ] Global Illumination (multiple techniques)

> More to follow.
