#include <cstdio>
#include <cstring>
#include <cmath>

#define CORE_IMPLEMENTATION
#define MATH_IMPLEMENTATION
#define KEYS_IMPLEMENTATION
#define CAMERA_IMPLEMENTATION
#define SDL_IMPLEMENTATION
#define IMGUI_IMPLEMENTATION
#define GPU_IMPLEMENTATION
#include "core.h"

#define WIDTH 1270
#define HEIGHT 850

typedef struct {
    float cam_pos[4];
    float cam_right[4];
    float cam_up[4];
    float cam_forward[4];
    float screen[4];
    float render_cfg[4];
} VoxelUniforms;

typedef struct {
    Window_t win;
    Camera cam;
    Input input;
    Gpu gpu;
    SDL_GPUGraphicsPipeline *pipeline;

    bool running;
    float ticks;
    float move_speed;
    float mouse_sensitivity;

    float grid_size;
    float max_dist;
    float max_steps;
} state_t;

static state_t state = {0};

static void render_callback(Gpu *gpu, SDL_GPUCommandBuffer *cmd, SDL_GPURenderPass *pass, GpuRenderData *data)
{
    const float aspect = (data->height > 0) ? ((float)data->width / (float)data->height) : 1.0f;
    const float tan_half_fov = tanf((state.cam.fov * PI / 180.0f) * 0.5f);

    const VoxelUniforms u = {
        .cam_pos     = {state.cam.position.x, state.cam.position.y, state.cam.position.z},
        .cam_right   = {state.cam.right.x,    state.cam.right.y,    state.cam.right.z},
        .cam_up      = {state.cam.up.x,       state.cam.up.y,       state.cam.up.z},
        .cam_forward = {state.cam.front.x,    state.cam.front.y,    state.cam.front.z},
        .screen      = {(float)data->width, (float)data->height, data->total_time, tan_half_fov},
        .render_cfg  = {aspect, state.grid_size, state.max_dist, state.max_steps},
    };

    SDL_PushGPUFragmentUniformData(cmd, 0, &u, (Uint32)sizeof(u));
    SDL_BindGPUGraphicsPipeline(pass, state.pipeline);
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
}

static void update()
{
    pollEvents(&state.win, &state.input);

    if (isKeyDown(&state.input, KEY_ESCAPE)) state.running = false;
    if (isKeyDown(&state.input, KEY_LSHIFT)) releaseMouse(state.win.window, &state.input);
    else if (!isMouseGrabbed(&state.input)) grabMouse(state.win.window, state.win.width, state.win.height, &state.input);

    float mx = 0, my = 0;
    SDL_GetRelativeMouseState(&mx, &my);
    if (isMouseGrabbed(&state.input)) cameraRotate(&state.cam, mx * state.mouse_sensitivity, -my * state.mouse_sensitivity);

    if (isKeyDown(&state.input, KEY_W)) cameraMove(&state.cam, state.cam.front, state.move_speed);
    if (isKeyDown(&state.input, KEY_S)) cameraMove(&state.cam, mul(state.cam.front, -1.0f), state.move_speed);
    if (isKeyDown(&state.input, KEY_A)) cameraMove(&state.cam, mul(state.cam.right, -1.0f), state.move_speed);
    if (isKeyDown(&state.input, KEY_D)) cameraMove(&state.cam, state.cam.right, state.move_speed);
}

static bool render()
{
    if (!state.pipeline) return false;

    imguiNewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::Begin("STATE", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs);
    ImGui::SeparatorText("Camera");
    ImGui::Text("Pos  %.2f  %.2f  %.2f", state.cam.position.x, state.cam.position.y, state.cam.position.z);
    ImGui::Text("Yaw  %.1f    Pitch  %.1f", state.cam.yaw, state.cam.pitch);
    ImGui::SeparatorText("Info");
    ImGui::Text("FPS: %.1f  |  Time: %.2fs", ImGui::GetIO().Framerate, state.ticks);
    ImGui::End();
    ImGui::Render();

    GpuRenderData data = { .delta_time = (float)getDelta(&state.win), .total_time = state.ticks };

    return gpuRenderFrame(&state.gpu, render_callback, &data);
}

int main()
{
    windowInit(&state.win);
    state.win.width  = WIDTH;
    state.win.height = HEIGHT;
    state.win.title  = "raycast";

    ASSERT(createWindow(&state.win));

    cameraInit(&state.cam);
    state.cam.position = vec3(10.0f, 14.0f, 24.0f);
    state.cam.yaw      = -114.0f;
    state.cam.pitch    = -40.0f;
    state.cam.fov      = 75.0f;
    cameraUpdate(&state.cam);

    inputInit(&state.input);

    ASSERT(gpuInit(&state.gpu, &state.win));

    state.pipeline = gpuCreatePipeline(&state.gpu, "voxel_raymarch", 0, 1);
    ASSERT(state.pipeline);

    imguiInit(&state.win, state.gpu.device, SDL_GetGPUSwapchainTextureFormat(state.gpu.device, state.win.window));

    state.grid_size  = 1.0f;
    state.max_dist   = 140.0f;
    state.max_steps  = 256.0f;

    state.running           = true;
    state.move_speed        = 0.1f;
    state.mouse_sensitivity = 0.3f;

    while (state.running) {
        update();
        state.ticks = (float)SDL_GetTicks() * 0.001f;
        if (!render()) state.running = false;
        updateFrame(&state.win);
    }

    gpuReleasePipeline(&state.gpu, &state.pipeline);
    gpuFree(&state.gpu);
    destroyWindow(&state.win);
    return 0;
}
