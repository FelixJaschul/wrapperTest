#!/usr/bin/env bash
set -euo pipefail

# WRITTEN BY AI

# Build all shaders in the repository
# Compiles GLSL -> SPIR-V -> MSL/DXIL (and metallib on macOS)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "$ROOT_DIR"

# =============================================================================
# CONFIGURATION
# =============================================================================
# Output directory for compiled shaders (can be overridden by first argument)
OUTPUT_DIR="${1:-shaders}"

# Space-separated list of directories containing shader files (.vert/.frag)
# Paths are relative to the repository root
SHADER_DIRS="shaders examples/sdl_gpu_voxel examples/sdl_gpu_triangle"
# =============================================================================

# Ensure output directory exists
mkdir -p "$OUTPUT_DIR"

find_glslang() {
  if command -v glslangValidator >/dev/null 2>&1; then
    command -v glslangValidator
    return 0
  fi
  if [[ -n "${VULKAN_SDK:-}" ]] && [[ -x "${VULKAN_SDK}/bin/glslangValidator" ]]; then
    echo "${VULKAN_SDK}/bin/glslangValidator"
    return 0
  fi
  if [[ -x "$HOME/VulkanSDK/1.4.335.1/macOS/bin/glslangValidator" ]]; then
    echo "$HOME/VulkanSDK/1.4.335.1/macOS/bin/glslangValidator"
    return 0
  fi
  return 1
}

build_shadercross_from_submodule() {
  local local_bin="${ROOT_DIR}/.build/shadercross/shadercross"
  if [[ -x "${local_bin}" ]]; then
    echo "${local_bin}"
    return 0
  fi
  return 1
}

GLSLANG_BIN="$(find_glslang || true)"
if [[ -z "${GLSLANG_BIN}" ]]; then
  echo "glslangValidator not found. Install Vulkan SDK or set VULKAN_SDK." >&2
  exit 1
fi

echo "Using glslangValidator: ${GLSLANG_BIN}"

SHADERCROSS_BIN=""
if command -v shadercross >/dev/null 2>&1; then
  SHADERCROSS_BIN="$(command -v shadercross)"
else
  SHADERCROSS_BIN="$(build_shadercross_from_submodule || true)"
fi

# Collect all shader base names (without extension) from all directories
declare -a ALL_SHADERS=()

for dir in $SHADER_DIRS; do
  if [[ ! -d "$dir" ]]; then
    echo "Warning: Shader directory '$dir' not found, skipping." >&2
    continue
  fi
  
  # Find all .vert and .frag files in this directory
  while IFS= read -r -d '' file; do
    base="$(basename "$file")"
    base="${base%.vert}"
    base="${base%.frag}"
    # Add to array if not already present
    found=0
    for existing in "${ALL_SHADERS[@]:-}"; do
      if [[ "$existing" == "$base" ]]; then
        found=1
        break
      fi
    done
    if [[ $found -eq 0 ]]; then
      ALL_SHADERS+=("$base")
    fi
  done < <(find "$dir" -maxdepth 1 -type f \( -name "*.vert" -o -name "*.frag" \) -print0 2>/dev/null || true)
done

if [[ ${#ALL_SHADERS[@]} -eq 0 ]]; then
  echo "No shaders found in configured directories: $SHADER_DIRS" >&2
  exit 1
fi

echo "Found shaders: ${ALL_SHADERS[*]}"
echo ""
echo "Compiling GLSL -> SPIR-V"

for shader in "${ALL_SHADERS[@]}"; do
  # Find the directory containing this shader
  shader_dir=""
  for dir in $SHADER_DIRS; do
    if [[ -f "$dir/$shader.vert" ]] || [[ -f "$dir/$shader.frag" ]]; then
      shader_dir="$dir"
      break
    fi
  done
  
  if [[ -z "$shader_dir" ]]; then
    echo "Warning: Could not find directory for shader '$shader', skipping." >&2
    continue
  fi
  
  echo "  Processing: $shader_dir/$shader"
  
    if [[ -f "$shader_dir/$shader.vert" ]]; then
      "${GLSLANG_BIN}" -V "$shader_dir/$shader.vert" -o "$OUTPUT_DIR/$shader.vert.spv"
    fi
    
    if [[ -f "$shader_dir/$shader.frag" ]]; then
      "${GLSLANG_BIN}" -V "$shader_dir/$shader.frag" -o "$OUTPUT_DIR/$shader.frag.spv"
    fi
done

if [[ -n "${SHADERCROSS_BIN}" ]]; then
  echo ""
  echo "Converting SPIR-V -> MSL/DXIL via shadercross"
  
  for shader in "${ALL_SHADERS[@]}"; do
    shader_dir=""
    for dir in $SHADER_DIRS; do
      if [[ -f "$dir/$shader.vert" ]] || [[ -f "$dir/$shader.frag" ]]; then
        shader_dir="$dir"
        break
      fi
    done
    
    if [[ -z "$shader_dir" ]]; then
      continue
    fi
    
    echo "  Processing: $shader_dir/$shader"
    
    if [[ -f "$OUTPUT_DIR/$shader.vert.spv" ]]; then
      "${SHADERCROSS_BIN}" "$OUTPUT_DIR/$shader.vert.spv" -s SPIRV -d MSL  -t vertex   -e main -o "$OUTPUT_DIR/$shader.vert.msl"
      "${SHADERCROSS_BIN}" "$OUTPUT_DIR/$shader.vert.spv" -s SPIRV -d DXIL -t vertex   -e main -o "$OUTPUT_DIR/$shader.vert.dxil"
    fi
    
    if [[ -f "$OUTPUT_DIR/$shader.frag.spv" ]]; then
      "${SHADERCROSS_BIN}" "$OUTPUT_DIR/$shader.frag.spv" -s SPIRV -d MSL  -t fragment -e main -o "$OUTPUT_DIR/$shader.frag.msl"
      "${SHADERCROSS_BIN}" "$OUTPUT_DIR/$shader.frag.spv" -s SPIRV -d DXIL -t fragment -e main -o "$OUTPUT_DIR/$shader.frag.dxil"
    fi
  done
else
  if command -v spirv-cross >/dev/null 2>&1; then
    echo ""
    echo "shadercross unavailable, using spirv-cross for MSL generation"
    
    for shader in "${ALL_SHADERS[@]}"; do
      shader_dir=""
      for dir in $SHADER_DIRS; do
        if [[ -f "$dir/$shader.vert" ]] || [[ -f "$dir/$shader.frag" ]]; then
          shader_dir="$dir"
          break
        fi
      done
      
      if [[ -z "$shader_dir" ]]; then
        continue
      fi
      
      if [[ -f "$OUTPUT_DIR/$shader.vert.spv" ]]; then
        spirv-cross "$OUTPUT_DIR/$shader.vert.spv" --msl --output "$OUTPUT_DIR/$shader.vert.msl"
      fi
      if [[ -f "$OUTPUT_DIR/$shader.frag.spv" ]]; then
        spirv-cross "$OUTPUT_DIR/$shader.frag.spv" --msl --output "$OUTPUT_DIR/$shader.frag.msl"
      fi
    done
  else
    echo ""
    echo "shadercross unavailable (not installed and no local build at .build/shadercross/shadercross); generated SPIR-V only."
  fi
fi

# Compile MSL to metallib on macOS
if command -v xcrun >/dev/null 2>&1; then
  if xcrun -sdk macosx -find metal >/dev/null 2>&1 && xcrun -sdk macosx -find metallib >/dev/null 2>&1; then
    echo ""
    echo "Compiling MSL -> metallib"
    
    for shader in "${ALL_SHADERS[@]}"; do
      shader_dir=""
      for dir in $SHADER_DIRS; do
        if [[ -f "$dir/$shader.vert" ]] || [[ -f "$dir/$shader.frag" ]]; then
          shader_dir="$dir"
          break
        fi
      done
      
      if [[ -z "$shader_dir" ]]; then
        continue
      fi
      
      for stage in vert frag; do
        if [[ -f "$OUTPUT_DIR/$shader.$stage.msl" ]]; then
          echo "  Compiling: $OUTPUT_DIR/$shader.$stage.msl"
          xcrun -sdk macosx metal -x metal -c "$OUTPUT_DIR/$shader.$stage.msl" -o "$OUTPUT_DIR/$shader.$stage.air"
          xcrun -sdk macosx metallib "$OUTPUT_DIR/$shader.$stage.air" -o "$OUTPUT_DIR/$shader.$stage.metallib"
        fi
      done
    done
  else
    echo ""
    echo "xcrun found but metal toolchain missing; skipping metallib generation."
  fi
fi

echo ""
echo "Done"
