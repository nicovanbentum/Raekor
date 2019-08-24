# Raekor Engine

![image](https://i.imgur.com/nXhVK2H.png)

### Windows
Use the supplied Visual Studio Project, binaries for SDL2 and Assimp are included.

### Linux (tested on Ubuntu 19.04) (**won't build, blame Assimp**)
**Install SDL2**

enter `sudo apt-get install libsdl2-2.0` in a terminal.

**Install Assimp**

Open a terminal and `cd` to `Raekor/dependencies/Assimp/`, do `cmake ./` then `make` then `sudo make install`.

**Run the Makefile**

`cd` to the Raekor directory and do `make rebuild` followed by `make run`.

### Render APIs
Supports both OpenGL and DirectX 11. At any point in time, one's development might be behind on the other.

> More to follow.
