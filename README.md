# Raekor Engine

![image](https://i.imgur.com/31qDNlr.png)

# Build

### Windows
Use the supplied Visual Studio Project, binaries for SDL2 and Assimp are included.

### Linux
enter `sudo apt-get install libsdl2-2.0` in a terminal.

Open a terminal and `cd` to `Raekor/dependencies/Assimp/`, do `cmake ./` then `make` then `sudo make install`.

`cd` to the Raekor directory and do `make rebuild`.

Run the application using `make run`.

>*Tested on Ubuntu 19.04*

# Features
Basic framework in place for DX11 and VK. Focus has shifted to getting features in fast in OpenGL so I can use the engine to  research and implement Global Illumination for a research course at the Hogeschool Utrecht. Some of the features currently being worked on:
- [X] Shader hotloading
- [X] Directional & Point lighting
- [X] Shadow mapping (point & directional)
- [X] Normal mapping
- [X] HDR, Gamma & Tone mapping
- [X] Deferred render pipeline
- [ ] PBR Material system
- [ ] Frostbite's Frame Graph design
- [X] Screen Space Ambient Occlusion
- [ ] Screen Space Reflections
- [ ] Global Illumination (multiple techniques)

> More to follow.
