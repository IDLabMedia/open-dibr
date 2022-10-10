# Open Realtime Depth Image Based Renderer (OpenDIBR)

This **Open Realtime Depth-Image-Based Renderer (OpenDIBR)** takes a **multi-view image/video dataset** as input and renders the view from the perspective of the viewer to the **desktop or VR display**. The viewer has full freedom of movement (**6DoF**) using the keyboard and mouse or the VR controllers.
Real-time performance is achieved through several optimizations, including the use of **CUDA, OpenGL and the NVidia Video Codec SDK**.
For the VR experience, the **OpenVR SDK** and **SteamVR** are used.

<img src="https://github.com/jlartois/dibr/blob/main/docs/group_posetrace.gif" width="49%"/> <img src="https://github.com/jlartois/dibr/blob/main/docs/painter_posetrace.gif" width="49%"/> 
<img src="https://github.com/jlartois/dibr/blob/main/docs/zen_garden_vr.gif" width="49%"/> <img src="https://github.com/jlartois/dibr/blob/main/docs/Barbershop_mirror_posetrace_dibr.gif" width="49%"/>

## Dependencies
- A C++11 capable compiler. The following options have been tested:
  - Windows: Visual Studio 2019
  - Ubuntu 18.04 LTS: gcc, g++
- CMake 3.7 or higher
- An NVidia GPU that supports hardware accelerated decoding for H265 4:2:0 (preferably up to 12-bit), [check your GPU here](https://en.wikipedia.org/wiki/Nvidia_NVDEC#GPU_support)
- [CUDA Toolkit 10.2 or higher](https://docs.nvidia.com/cuda/cuda-installation-guide-microsoft-windows/index.html#installing-cuda-development-tools)
- [NVidia Video Codec SDK](https://developer.nvidia.com/nvidia-video-codec-sdk)

If you want to use Virtual Reality:
- Steam and SteamVR
- The following HMDs have been tested:
  - HTC Vive
  - HTV Vive Pro
  - Oculus Rift S

For Linux systems, there are several dependencies on libraries that need to be installed: FFmpeg, OpenGL, GLEW and SDL2. For example on Ubuntu:
```
sudo apt update
sudo apt install -y pkg-config libavcodec-dev libavformat-dev libavutil-dev libswresample-dev libglew-dev libsdl2-2.0
```

## Build from source
The following build instructions were tested on Windows 10 and Ubuntu 18.04 LTS:

Clone the repository:
```
git clone TODO
cd TODO
git submodule update --init --recursive
```
Use CMake to build the project:
```
cmake . -B bin
cmake --build bin --config Release
```
If the build was succesful, you now have an executable. 

Read [Running the Application](https://github.com/jlartois/dibr/wiki/Running-the-application) for a detailed overview of **how to run and use the application**. It also has a section on [Common Problems (scroll down)](https://github.com/jlartois/dibr/wiki/Running-the-application) that you might run into.

Read [Light field datasets](https://github.com/jlartois/dibr/wiki/Light-field-datasets) to **download existing light field datasets or adapt your own** to use as input for the application.

<img src="https://github.com/jlartois/dibr/blob/main/docs/overview_mpegi_datasets.png" /> 
<img src="https://github.com/jlartois/dibr/blob/main/docs/overview_idlab_datasets.png" /> 

A paper on this software tool is currently being written/evaluated. Instructions on how to **replicate the results from the paper** can be found [here](https://github.com/jlartois/dibr/wiki/Replicating-results-from-paper).


## Cite us

We do not have an official citation ready yet. 

Dataset and paper by [IDLab MEDIA](https://media.idlab.ugent.be/).
