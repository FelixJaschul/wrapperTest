# implc

Example repository demonstrating usage of my [wrapper](https://github.com/FelixJaschul/wrapper) library for SDL3-based graphics applications.

## Building

### Prerequisites

- CMake 3.20+
- A C++20 compatible compiler
- Vulkan SDK (for `glslangValidator`) — required for shader compilation

### Build Steps

1. **Clone the repository recursively:**
   ```bash
   git clone --recursive https://github.com/FelixJaschul/implc.git
   cd implc
   ```

2. **Build shadercross (required before building the game):**
   ```bash
   ./scripts/build_shadercross.sh
   ```
   This builds the `SDL_shadercross` tool from the submodule, which is needed to convert shaders to Metal (MSL) and DirectX (DXIL) formats.

3. **Configure and build with CMake:**
   ```bash
   cmake -B cmake-build-debug -S .
   cmake --build cmake-build-debug
   ```

   Or use the provided Makefile:
   ```bash
   make
   ```

## Examples

This repository includes example applications demonstrating different rendering techniques:

### `examples/sdl_gpu_voxel` — Voxel Raymarching (GPU)

First-person voxel raymarching demo with procedural voxel generation using SDL_GPU.

**Files:**
- `sdl_gpu_voxel.c` — Main application with camera controls and uniform submission
- `voxel_raymarch.vert` — Full-screen triangle vertex shader
- `voxel_raymarch.frag` — Raymarching fragment shader with DDA voxel traversal

**Controls:**
- `WASD` — Move camera
- `Mouse` — Look around (press `LShift` to release cursor)

**Build target:**
```bash
cmake --build cmake-build-debug --target sdl_gpu_voxel
```

### `examples/sdl_cpu_voxel` — Voxel Raymarching (CPU)

Same voxel raymarching demo but using CPU-based software rendering with SDL_Renderer.

**Build target:**
```bash
cmake --build cmake-build-debug --target sdl_cpu_voxel
```

### `examples/sdl_gpu_triangle` — Basic Triangle (GPU)

Simple colored triangle rendering example using SDL_GPU with GPU shaders.

**Files:**
- `sdl_gpu_triangle.c` — Main application with triangle vertex/fragment uniforms
- `basic_triangle.vert` — Vertex shader with position and color attributes
- `basic_triangle.frag` — Fragment shader with global tint uniform

**Build target:**
```bash
cmake --build cmake-build-debug --target sdl_gpu_triangle
```

### `examples/sdl_cpu_triangle` — Basic Triangle (CPU)

Same triangle example but using CPU-based software rendering with SDL_Renderer.

**Build target:**
```bash
cmake --build cmake-build-debug --target sdl_cpu_triangle
```

## Project Structure

```
implc/
├── CMakeLists.txt          # Main build configuration
├── scripts/
│   ├── build_shadercross.sh  # Builds SDL_shadercross from submodule
│   └── build_shaders.sh      # Compiles all shaders in the repo
├── shaders/                # Shared/common shaders
├── examples/
│   ├── sdl_gpu_voxel/      # Voxel raymarching (SDL_GPU)
│   │   ├── sdl_gpu_voxel.c
│   │   ├── voxel_raymarch.vert
│   │   └── voxel_raymarch.frag
│   ├── sdl_cpu_voxel/      # Voxel raymarching (SDL_Renderer, CPU)
│   │   └── sdl_cpu_voxel.c
│   ├── sdl_gpu_triangle/   # Basic triangle (SDL_GPU)
│   │   ├── sdl_gpu_triangle.c
│   │   ├── basic_triangle.vert
│   │   └── basic_triangle.frag
│   └── sdl_cpu_triangle/   # Basic triangle (SDL_Renderer, CPU)
│       └── sdl_cpu_triangle.c
└── wrapper/                # SDL3 wrapper library (submodule)
```

## Adding New Examples

1. **Create a directory** under `examples/` with your example name:
   ```bash
   mkdir examples/my_example
   ```

2. **Add your source file** — must be named `<dirname>.c`:
   ```bash
   # examples/my_example/my_example.c
   ```

3. **Add shaders** (optional, for GPU examples) in the same directory:
   ```bash
   # examples/my_example/my_shader.vert
   # examples/my_example/my_shader.frag
   ```

4. **Update `CMakeLists.txt`** — add your directory to `EXAMPLE_DIRS`:
   ```cmake
   set(EXAMPLE_DIRS "examples/sdl_gpu_voxel" "examples/sdl_cpu_voxel" 
                    "examples/sdl_gpu_triangle" "examples/sdl_cpu_triangle"
                    "examples/my_example")
   ```

5. **Build** — your example will be compiled automatically:
   ```bash
   cmake --build cmake-build-debug --target my_example
   ```

## Adding New Shaders

The build system automatically compiles all shaders in configured directories. To add new shaders:

1. **Place shader files** in any of the configured directories (or add a new directory)
2. **Update `scripts/build_shaders.sh`** — add your directory to the `SHADER_DIRS` variable:
   ```bash
   SHADER_DIRS="shaders examples/sdl_gpu_voxel examples/sdl_gpu_triangle your/new/dir"
   ```
3. **Run the build** — shaders are compiled automatically during CMake build

Shader files must be named `<name>.vert` and/or `<name>.frag` to be detected.

## Notes

- The shader compilation step (`scripts/build_shaders.sh`) runs automatically as part of the CMake build, but it depends on `shadercross` being built first (step 2).
- If you skip step 2, shaders will only be compiled to SPIR-V format, and Metal/DirectX support will be missing.
- On macOS, Metal shaders (`.metallib`) will be compiled automatically if Xcode command line tools are installed.
- All generated shader files (`.spv`, `.msl`, `.dxil`, `.metallib`) are placed in the `shaders/` directory inside the CMake build folder (e.g., `cmake-build-debug/shaders/`).
