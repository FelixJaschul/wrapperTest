#include <cstdint>
#include <cstdio>
#include <cmath>

#define CORE_IMPLEMENTATION
#define MATH_IMPLEMENTATION
#define KEYS_IMPLEMENTATION
#define CAMERA_IMPLEMENTATION
#define SDL_IMPLEMENTATION
#include "core.h"

#define WIDTH 1270
#define HEIGHT 850

typedef struct {
    SDL_GPUDevice *device;
    SDL_GPUGraphicsPipeline *pipeline;
    const char *driver_name;
} gpu_state_t;

typedef struct {
    Window_t win;

    Camera cam;
    Input input;

    bool running;
    float delta;
    float ticks;
    float move_speed;
    float mouse_sensitivity;

    gpu_state_t gpu;
} state_t;

state_t state = {0};

#define cleanup() do { \
    if (state.gpu.pipeline) SDL_ReleaseGPUGraphicsPipeline(state.gpu.device, state.gpu.pipeline); \
    if (state.gpu.device) { \
        if (state.win.window) SDL_ReleaseWindowFromGPUDevice(state.gpu.device, state.win.window); \
        SDL_DestroyGPUDevice(state.gpu.device); \
    } \
    destroyWindow(&state.win); \
} while(0)

typedef struct {
    float cam_pos[4];
    float cam_right[4];
    float cam_up[4];
    float cam_forward[4];
    float screen_time[4];
    float render_cfg[4];
} FragmentUniforms;

typedef struct {
    Uint8 *code;
    size_t size;
    SDL_GPUShaderFormat format;
} LoadedShader;

static bool load_file_with_fallback(const char *path, Uint8 **data, size_t *size, bool log_errors)
{
    if (!path || !data || !size) return false;

    *data = (Uint8 *)SDL_LoadFile(path, size);
    if (*data && *size > 0) return true;

    char full[1024];
    char rel[1024];
    char rel2[1024];
    const char *base = SDL_GetBasePath();

    if (base) {
        SDL_snprintf(full, sizeof(full), "%s%s", base, path);
        *data = (Uint8 *)SDL_LoadFile(full, size);
        if (*data && *size > 0) {
            return true;
        }

        SDL_snprintf(rel, sizeof(rel), "%s../%s", base, path);
        *data = (Uint8 *)SDL_LoadFile(rel, size);
        if (*data && *size > 0) {
            return true;
        }

        SDL_snprintf(rel2, sizeof(rel2), "%s../../%s", base, path);
        *data = (Uint8 *)SDL_LoadFile(rel2, size);
        if (*data && *size > 0) {
            return true;
        }
    }

    return false;
}

static bool load_shader_for_device(bool is_vertex, LoadedShader *out_shader)
{
    out_shader->code = nullptr;
    out_shader->size = 0;
    out_shader->format = SDL_GPU_SHADERFORMAT_INVALID;

    const SDL_GPUShaderFormat supported = SDL_GetGPUShaderFormats(state.gpu.device);
    const char *stage = is_vertex ? "vert" : "frag";
    char path[256];

    if (supported & SDL_GPU_SHADERFORMAT_SPIRV) {
        SDL_snprintf(path, sizeof(path), "shaders/voxel_raymarch.%s.spv", stage);
        if (load_file_with_fallback(path, &out_shader->code, &out_shader->size, false)) {
            out_shader->format = SDL_GPU_SHADERFORMAT_SPIRV;
            return true;
        }
    }

    if (supported & SDL_GPU_SHADERFORMAT_METALLIB) {
        SDL_snprintf(path, sizeof(path), "shaders/voxel_raymarch.%s.metallib", stage);
        if (load_file_with_fallback(path, &out_shader->code, &out_shader->size, false)) {
            out_shader->format = SDL_GPU_SHADERFORMAT_METALLIB;
            return true;
        }
    }

    if (supported & SDL_GPU_SHADERFORMAT_MSL) {
        SDL_snprintf(path, sizeof(path), "shaders/voxel_raymarch.%s.msl", stage);
        if (load_file_with_fallback(path, &out_shader->code, &out_shader->size, false)) {
            out_shader->format = SDL_GPU_SHADERFORMAT_MSL;
            return true;
        }
    }

    if (supported & SDL_GPU_SHADERFORMAT_DXIL) {
        SDL_snprintf(path, sizeof(path), "shaders/voxel_raymarch.%s.dxil", stage);
        if (load_file_with_fallback(path, &out_shader->code, &out_shader->size, false)) {
            out_shader->format = SDL_GPU_SHADERFORMAT_DXIL;
            return true;
        }
    }

    if (supported & SDL_GPU_SHADERFORMAT_DXBC) {
        SDL_snprintf(path, sizeof(path), "shaders/voxel_raymarch.%s.dxbc", stage);
        if (load_file_with_fallback(path, &out_shader->code, &out_shader->size, false)) {
            out_shader->format = SDL_GPU_SHADERFORMAT_DXBC;
            return true;
        }
    }

    std::fprintf(stderr, "No compatible shader binary found for stage '%s'. Supported format mask: 0x%x\n", stage, (unsigned)supported);
    return false;
}

static bool init_gpu_pipeline()
{
    // Prefer Vulkan but allow fallback so the same app runs on Metal/D3D12 when available.
    const SDL_GPUShaderFormat wanted_formats =
        SDL_GPU_SHADERFORMAT_SPIRV |
        SDL_GPU_SHADERFORMAT_MSL |
        SDL_GPU_SHADERFORMAT_DXIL |
        SDL_GPU_SHADERFORMAT_DXBC;

    state.gpu.device = SDL_CreateGPUDevice(wanted_formats, true, "vulkan");
    if (!state.gpu.device) {
        state.gpu.device = SDL_CreateGPUDevice(wanted_formats, true, nullptr);
        if (!state.gpu.device) {
            std::fprintf(stderr, "SDL_CreateGPUDevice failed: %s\n", SDL_GetError());
            return false;
        }
    }

    state.gpu.driver_name = SDL_GetGPUDeviceDriver(state.gpu.device);
    std::printf("SDL GPU driver: %s\n", state.gpu.driver_name ? state.gpu.driver_name : "unknown");

    if (!SDL_ClaimWindowForGPUDevice(state.gpu.device, state.win.window)) {
        std::fprintf(stderr, "SDL_ClaimWindowForGPUDevice failed: %s\n", SDL_GetError());
        return false;
    }

    if (!SDL_SetGPUSwapchainParameters(
            state.gpu.device,
            state.win.window,
            SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
            SDL_GPU_PRESENTMODE_VSYNC)) {
        std::fprintf(stderr, "SDL_SetGPUSwapchainParameters failed: %s\n", SDL_GetError());
        return false;
    }

    LoadedShader vert_src = {};
    LoadedShader frag_src = {};
    if (!load_shader_for_device(true, &vert_src)) return false;
    if (!load_shader_for_device(false, &frag_src)) {
        SDL_free(vert_src.code);
        return false;
    }

    auto create_shader_with_entry_fallback = [](SDL_GPUDevice *device, SDL_GPUShaderCreateInfo *info) -> SDL_GPUShader * {
        if (info->format == SDL_GPU_SHADERFORMAT_MSL || info->format == SDL_GPU_SHADERFORMAT_METALLIB)
            info->entrypoint = "main0";
        else info->entrypoint = "main";

        SDL_GPUShader *shader = SDL_CreateGPUShader(device, info);
        if (shader) return shader;

        if (info->format == SDL_GPU_SHADERFORMAT_MSL || info->format == SDL_GPU_SHADERFORMAT_METALLIB) {
            info->entrypoint = "main";
            shader = SDL_CreateGPUShader(device, info);
        }
        return shader;
    };

    SDL_GPUShaderCreateInfo vinfo = {};
    vinfo.code = vert_src.code;
    vinfo.code_size = vert_src.size;
    vinfo.format = vert_src.format;
    vinfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;

    SDL_GPUShaderCreateInfo finfo = {};
    finfo.code = frag_src.code;
    finfo.code_size = frag_src.size;
    finfo.format = frag_src.format;
    finfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    finfo.num_uniform_buffers = 1;

    SDL_GPUShader *vert = create_shader_with_entry_fallback(state.gpu.device, &vinfo);
    SDL_GPUShader *frag = create_shader_with_entry_fallback(state.gpu.device, &finfo);

    SDL_free(vert_src.code);
    SDL_free(frag_src.code);

    if (!vert || !frag) {
        if (vert) SDL_ReleaseGPUShader(state.gpu.device, vert);
        if (frag) SDL_ReleaseGPUShader(state.gpu.device, frag);
        std::fprintf(stderr, "SDL_CreateGPUShader failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GPUColorTargetDescription color_target = {};
    color_target.format = SDL_GetGPUSwapchainTextureFormat(state.gpu.device, state.win.window);

    SDL_GPUGraphicsPipelineCreateInfo pinfo = {};
    pinfo.vertex_shader = vert;
    pinfo.fragment_shader = frag;
    pinfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pinfo.target_info.num_color_targets = 1;
    pinfo.target_info.color_target_descriptions = &color_target;
    pinfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pinfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pinfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    pinfo.rasterizer_state.enable_depth_clip = true;

    state.gpu.pipeline = SDL_CreateGPUGraphicsPipeline(state.gpu.device, &pinfo);

    SDL_ReleaseGPUShader(state.gpu.device, vert);
    SDL_ReleaseGPUShader(state.gpu.device, frag);

    if (!state.gpu.pipeline) {
        std::fprintf(stderr, "SDL_CreateGPUGraphicsPipeline failed: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

static void update()
{
    if (pollEvents(&state.win, &state.input)) {
        state.running = false;
        return;
    }

    if (isKeyDown(&state.input, KEY_LSHIFT)) releaseMouse(state.win.window, &state.input);
    else if (!isMouseGrabbed(&state.input)) grabMouse(state.win.window, state.win.width, state.win.height, &state.input);

    int dx, dy;
    getMouseDelta(&state.input, &dx, &dy);
    cameraRotate(&state.cam, (float)dx * state.mouse_sensitivity, (float)(-dy) * state.mouse_sensitivity);

    if (isKeyDown(&state.input, KEY_W)) cameraMove(&state.cam, state.cam.front, state.move_speed);
    if (isKeyDown(&state.input, KEY_S)) cameraMove(&state.cam, mul(state.cam.front, -1.0f), state.move_speed);
    if (isKeyDown(&state.input, KEY_A)) cameraMove(&state.cam, mul(state.cam.right, -1.0f), state.move_speed);
    if (isKeyDown(&state.input, KEY_D)) cameraMove(&state.cam, state.cam.right, state.move_speed);
}

static bool render_frame_gpu()
{
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state.gpu.device);
    if (!cmd) {
        std::fprintf(stderr, "SDL_AcquireGPUCommandBuffer failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GPUTexture *swapchain = nullptr;
    Uint32 swap_w = 0, swap_h = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, state.win.window, &swapchain, &swap_w, &swap_h)) {
        std::fprintf(stderr, "SDL_WaitAndAcquireGPUSwapchainTexture failed: %s\n", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        return false;
    }

    if (!swapchain) {
        SDL_CancelGPUCommandBuffer(cmd);
        return true;
    }

    const float width = (float)swap_w;
    const float height = (float)swap_h;
    const float aspect = (height > 0.0f) ? (width / height) : 1.0f;
    const float tan_half_fov = tanf((float)(state.cam.fov * M_PI / 180.0f) * 0.5f);

    FragmentUniforms u = {};
    u.cam_pos[0] = state.cam.position.x;
    u.cam_pos[1] = state.cam.position.y;
    u.cam_pos[2] = state.cam.position.z;

    u.cam_right[0] = state.cam.right.x;
    u.cam_right[1] = state.cam.right.y;
    u.cam_right[2] = state.cam.right.z;

    u.cam_up[0] = state.cam.up.x;
    u.cam_up[1] = state.cam.up.y;
    u.cam_up[2] = state.cam.up.z;

    u.cam_forward[0] = state.cam.front.x;
    u.cam_forward[1] = state.cam.front.y;
    u.cam_forward[2] = state.cam.front.z;

    u.screen_time[0] = width;
    u.screen_time[1] = height;
    u.screen_time[2] = state.ticks * 0.001f;
    u.screen_time[3] = tan_half_fov;

    u.render_cfg[0] = aspect;
    u.render_cfg[1] = 1.0f;   // grid size
    u.render_cfg[2] = 140.0f; // max trace distance
    u.render_cfg[3] = 256.0f; // max dda steps

    SDL_PushGPUFragmentUniformData(cmd, 0, &u, (Uint32)sizeof(u));

    SDL_GPUColorTargetInfo target = {};
    target.texture = swapchain;
    target.clear_color.r = 0.08f;
    target.clear_color.g = 0.10f;
    target.clear_color.b = 0.12f;
    target.clear_color.a = 1.0f;
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &target, 1, nullptr);
    SDL_BindGPUGraphicsPipeline(pass, state.gpu.pipeline);
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(pass);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        std::fprintf(stderr, "SDL_SubmitGPUCommandBuffer failed: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

int main()
{
    windowInit(&state.win);
    state.win.width = WIDTH;
    state.win.height = HEIGHT;
    state.win.title = "raycast";

    ASSERT(createWindow(&state.win));

    if (state.win.renderer) {
        SDL_DestroyRenderer(state.win.renderer);
        state.win.renderer = nullptr;
    }

    cameraInit(&state.cam);
    state.cam.position = vec3(0.0f, 3.0f, 10.0f);
    state.cam.yaw = -90.0f;
    state.cam.pitch = -20.0f;
    state.cam.fov = 75.0f;
    cameraUpdate(&state.cam);

    inputInit(&state.input);

    ASSERT(init_gpu_pipeline());

    state.running = true;
    state.move_speed = 0.1f;
    state.mouse_sensitivity = 0.3f;

    while (state.running) {
        update();

        state.delta = (float)getDelta(&state.win);
        state.ticks = (float)SDL_GetTicks();

        if (!render_frame_gpu()) {
            state.running = false;
        }

        updateFrame(&state.win);
    }

    cleanup();
    return 0;
}
