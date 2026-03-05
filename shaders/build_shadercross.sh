#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SUBMODULE_DIR="${ROOT_DIR}/wrapper/libs/SDL_shadercross"
BUILD_DIR="${ROOT_DIR}/.build/shadercross"
SUPER_DIR="${BUILD_DIR}/super"
SUPER_BUILD="${BUILD_DIR}/super-build"

if [[ ! -f "${SUBMODULE_DIR}/CMakeLists.txt" ]]; then
  echo "SDL_shadercross submodule not found at wrapper/libs/SDL_shadercross" >&2
  echo "Run: git submodule update --init --recursive" >&2
  exit 1
fi

mkdir -p "${SUPER_DIR}"
cat > "${SUPER_DIR}/CMakeLists.txt" <<CMAKE
cmake_minimum_required(VERSION 3.22)
project(shadercross_super C CXX)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("${ROOT_DIR}/wrapper/libs/SDL" sdl EXCLUDE_FROM_ALL)
set(SDLSHADERCROSS_VENDORED ON CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_CLI ON CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_SHARED OFF CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_STATIC ON CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${SUBMODULE_DIR}" shadercross EXCLUDE_FROM_ALL)
CMAKE

cmake -S "${SUPER_DIR}" -B "${SUPER_BUILD}"
cmake --build "${SUPER_BUILD}" --target shadercross -j

BIN="${SUPER_BUILD}/shadercross/shadercross"
if [[ ! -x "${BIN}" ]]; then
  BIN="$(find "${SUPER_BUILD}" -type f -name shadercross | head -n 1 || true)"
fi

if [[ -z "${BIN}" ]] || [[ ! -x "${BIN}" ]]; then
  echo "Built, but shadercross binary was not found." >&2
  exit 1
fi

mkdir -p "${BUILD_DIR}"
cp -f "${BIN}" "${BUILD_DIR}/shadercross"
chmod +x "${BUILD_DIR}/shadercross"

echo "shadercross ready at ${BUILD_DIR}/shadercross"
