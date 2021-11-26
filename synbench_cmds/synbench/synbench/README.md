# SynBench - OS-agnostic OpenGL 3D benchmark

Reference 3D benchmark for performance comparison and optimization This project can be built in both Windows and Linux
environment. Core OpenGL portion is derived from "stress-weston" but to make it compatible on both windows and linux, it
uses SDL.

Current Version: 0.5

**Pre-requisites**

Windows:

1. Visual Studio 2019 (older version was not tested)

2. The OpenGL Extension Wrangler Library (GLEW)

   a. Download binary + headers
   from: https://sourceforge.net/projects/glew/files/glew/2.1.0/glew-2.1.0-win32.zip/download

   b. Unzip zip file and copy win32 dll and lib files (e.g glew-2.1.0-win32\glew-2.1.0\bin\Release\Win32\glew32.dll,
   glew-2.1.0-win32\glew-2.1.0\lib\Release\Win32\glew32.lib) to the project folder (e.g ./synbench)

   c. Place header files folder (e.g. glew-2.1.0-win32\glew-2.1.0\include\GL) to under "C:\Program Files (x86)\Windows
   Kits\10\Include\10.0.18362.0\um\"

3. Simple DirectMedia Layer (SDL)

   a. Download binary + headers from: https://www.libsdl.org/release/SDL2-devel-2.0.14-VC.zip

   b. Unzip zip file and copy dll and lib files (e.g SDL2-2.0.12\lib\x86\SDL2.dll and SDL2-2.0.12\lib\x86\SDL2.lib) to
   the project folder (e.g ./synbench)

   c. Create "SDL2" folder under "C:\Program Files (x86)\Windows Kits\10\Include\10.0.18362.0\um\" and copy all header
   files under include folder (e.g. SDL2-2.0.12\include) to "C:\Program Files (x86)\Windows
   Kits\10\Include\10.0.18362.0\um\SDL2"

4. OpenGL Mathematics (GLM)

   a. Download GLM release from: https://github.com/g-truc/glm/releases/download/0.9.9.8/glm-0.9.9.8.zip

   b. Unzip zip file and copy "glm-0.9.9.8\glm\glm" to "C:\Program Files (x86)\Windows Kits\10\Include\10.0.18362.0\um\"

Linux:

Using Ubuntu

1. Basic GNU toolchain
2. The OpenGL Extension Wrangler Library (GLEW)

   To install libglew-dev and its dependencies

        sudo apt-get install libglew-dev

3. Simple DirectMedia Layer (SDL)

   To install libsdl2-dev and its dependencies

        sudo apt-get install libsdl2-dev

4. OpenGL Mathematics (GLM)

   To install libglm-dev and its dependencies

        sudo apt-get install libglm-dev

**How to Build**

Windows

1. Open visual studio project file, "synbench\synbench.vcxproj"

2. Build

Linux:

1. `make`

2. `export DESTDIR=$PWD` or `export DESTDIR=/usr/local` for example to indicate where to install binaries and params/*
   folder

3. `make install`

NOTE: `export DESTDIR` is crucial if you are going to use the WLC KPI Runner Framework to invoke `synbench`

**How to Run**

    a. Execute "synbench.exe" on windows or "./synbench" on Linux (this will invoke default params file default_params.txt)

    b. To run with a specific params file, "./synbench <params/<file of choice>"
    
    c. Edit input parameters inside "params.txt" as needed before invoking synbench
