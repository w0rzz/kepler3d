# Kepler3D

Kepler3D is an OpenGL and C++11 game engine.

This is a learning experience and a work in progress so I don't recommend using this engine in its current state.

**Features:**

- OpenGL 4.2, C++11
- [glTF](https://github.com/KhronosGroup/glTF) scene loader and renderer
- Bitmap font loader and renderer (using the [Angel Code format](http://www.angelcode.com/products/bmfont/))
- Uses [GLFW](http://www.glfw.org/) for platform abstraction
- Unit tests using [google test](https://github.com/google/googletest)

# Building

## Windows

**Requirements:**

- 64 bit Windows
- Visual Studio 2015

External-dependencies and resource files are stored separately to keep the repo size down.

### Setup

- Clone the repo
- Download [kepler3d_deps.zip](https://github.com/dgough/kepler3d/releases/download/v0.0.4/kepler3d_deps.zip) from [releases](https://github.com/dgough/kepler3d/releases) and unzip to the kepler3d directory
- Open **kepler3d.sln**

# FAQ

### Should I use this engine to make a game?

No. It is very incomplete and unoptimized.

### Are you accepting outside contributions?

No. I want to stay as the sole contributor for now.

# Dependencies

- [glad](https://github.com/Dav1dde/glad)
- [GLFW](http://www.glfw.org/)
- [glm](http://glm.g-truc.net)
- [SOIL2](https://bitbucket.org/SpartanJ/soil2)
- [nlohmann/json](https://github.com/nlohmann/json)
- [libb64](http://libb64.sourceforge.net)
- [google test](https://github.com/google/googletest)
