# Shader Editing Guide

## Building shaders

**Before building the game**, run:
```bash
./shaders/build_shadercross.sh
```

This builds the `shadercross` tool needed for shader conversion. The main shader compilation (`build_shaders.sh`) runs automatically during the CMake build.

## Shader responsibilities

- `voxel_raymarch.vert/.frag`: raymarch + shading from uniforms
- `basic_triangle.vert/.frag`: draw/shade triangle from uniforms

## Uniform flow

- `main.c` fills its local voxel uniform struct and sends it to voxel fragment shader
- `tri.c` fills vertex + fragment uniform structs and sends them each frame
- Wrapper only submits uniforms and draw calls
