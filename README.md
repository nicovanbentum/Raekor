# Raekor Engine

### Windows
Use the supplied Visual Studio Project.

the `assimp-build` folder will be missing, since it's too large for Github. You'll have to either compile assimp yourself (static library, 64-bit) or download my build folder (link coming soon). Under `dependencies` create a new folder called `assimp-build` and make sure it contains the following files:

![](https://i.imgur.com/ouATnNt.png)

The assimp folder here should only contain the generated `config.h` from the assimp build.

### Linux (tested on Ubuntu 19.04) (**ASSIMP BROKE THE LINUX BUILD, CHECK BACK LATER**)
Use the supplied Makefile. `cd` to the Raekor directory and enter `make rebuild` followed by `make run`.
Requires **SDL2** to be installed by entering `sudo apt-get install libsdl2-2.0` .

### Render APIs
Supports both OpenGL and DirectX 11. At any point in time, one's development might be behind on the other.

> More to follow.
