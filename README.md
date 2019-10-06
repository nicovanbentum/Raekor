# Raekor Engine

![image](https://i.imgur.com/nXhVK2H.png)

# How to build

### Windows
Use the supplied Visual Studio Project, binaries for SDL2 and Assimp are included.

### Linux
enter `sudo apt-get install libsdl2-2.0` in a terminal.

Open a terminal and `cd` to `Raekor/dependencies/Assimp/`, do `cmake ./` then `make` then `sudo make install`.

`cd` to the Raekor directory and do `make rebuild`.

Run the application using `make run`.

>*Tested on Ubuntu 19.04*

### Render APIs
Supports both OpenGL and DirectX 11. At any point in time, one's development might be behind on the other.
Vulkan is in the works but will take a significant rewrite of every other API's abstractions.

> More to follow.
