boot_with_shaders: init build run

g = sdl_cpu_triangle
# make g=sdl_gpu_triangle run ...
# g=sdl_cpu_triangel
# g=sdl_gpu_triangel
# g=sdl_gpu_voxel

init:
	git submodule update --init --recursive
	./scripts/build_shadercross.sh || true

build:
	mkdir -p cmake-build-debug
	cd cmake-build-debug && cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	cmake --build cmake-build-debug -- -j$$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
	ln -sf cmake-build-debug/compile_commands.json compile_commands.json

run:
	./cmake-build-debug/$(g)

clean:
	rm -rf cmake-build-debug compile_commands.json .build imgui.ini
