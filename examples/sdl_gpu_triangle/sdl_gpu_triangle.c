#include <cstdio>

#define CORE_IMPLEMENTATION
#define MATH_IMPLEMENTATION
#define KEYS_IMPLEMENTATION
#define CAMERA_IMPLEMENTATION
#define SDL_IMPLEMENTATION
#include "core.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"

#define WIDTH 1270
#define HEIGHT 850

typedef struct {
    float positions[3][4];
    float colors[3][4];
} TriangleVertexUniforms;

typedef struct {
    float global_tint[4];
} TriangleFragmentUniforms;

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

    float positions[3][2];
    float colors[3][3];
    float tint[3];
} state_t;

static state_t state = {0};

#define cleanup() do { \
    ImGui_ImplSDLGPU3_Shutdown(); \
    ImGui_ImplSDL3_Shutdown(); \
    ImGui::DestroyContext(); \
    gpuReleasePipeline(&state.gpu, &state.pipeline); \
    gpuFree(&state.gpu); \
    destroyWindow(&state.win); \
} while(0)

static void reset_triangle()
{
    state.positions[0][0] =  0.0f;  state.positions[0][1] =  0.72f;
    state.positions[1][0] = -0.72f; state.positions[1][1] = -0.72f;
    state.positions[2][0] =  0.72f; state.positions[2][1] = -0.72f;
    state.colors[0][0] = 1.00f; state.colors[0][1] = 0.30f; state.colors[0][2] = 0.20f;
    state.colors[1][0] = 0.15f; state.colors[1][1] = 0.85f; state.colors[1][2] = 0.35f;
    state.colors[2][0] = 0.15f; state.colors[2][1] = 0.40f; state.colors[2][2] = 1.00f;
    state.tint[0] = state.tint[1] = state.tint[2] = 1.0f;
}

static void update()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT) state.running = false;
    }

    const bool *keys = SDL_GetKeyboardState(NULL);
    if (keys[SDL_SCANCODE_LSHIFT]) releaseMouse(state.win.window, &state.input);
    else if (!isMouseGrabbed(&state.input)) grabMouse(state.win.window, state.win.width, state.win.height, &state.input);

    float mx = 0, my = 0;
    SDL_GetRelativeMouseState(&mx, &my);
    if (isMouseGrabbed(&state.input)) cameraRotate(&state.cam, mx * state.mouse_sensitivity, -my * state.mouse_sensitivity);

    if (keys[SDL_SCANCODE_W]) cameraMove(&state.cam, state.cam.front, state.move_speed);
    if (keys[SDL_SCANCODE_S]) cameraMove(&state.cam, mul(state.cam.front, -1.0f), state.move_speed);
    if (keys[SDL_SCANCODE_A]) cameraMove(&state.cam, mul(state.cam.right, -1.0f), state.move_speed);
    if (keys[SDL_SCANCODE_D]) cameraMove(&state.cam, state.cam.right, state.move_speed);
}

static bool render()
{
    if (!state.pipeline) return false;

    const TriangleVertexUniforms v = {
        .positions = {
            {state.positions[0][0], state.positions[0][1], 0.0f, 0.0f},
            {state.positions[1][0], state.positions[1][1], 0.0f, 0.0f},
            {state.positions[2][0], state.positions[2][1], 0.0f, 0.0f},
        },
        .colors = {
            {state.colors[0][0], state.colors[0][1], state.colors[0][2], 0.0f},
            {state.colors[1][0], state.colors[1][1], state.colors[1][2], 0.0f},
            {state.colors[2][0], state.colors[2][1], state.colors[2][2], 0.0f},
        },
    };

    const TriangleFragmentUniforms f = {
        .global_tint = {state.tint[0], state.tint[1], state.tint[2], 0.0f},
    };

    // 1. Build ImGui draw lists
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 330), ImGuiCond_FirstUseEver);
    ImGui::Begin("Triangle Editor");

    ImGui::SeparatorText("Vertex Positions (NDC)");
    ImGui::SliderFloat2("Vertex 0", state.positions[0], -1.0f, 1.0f);
    ImGui::SliderFloat2("Vertex 1", state.positions[1], -1.0f, 1.0f);
    ImGui::SliderFloat2("Vertex 2", state.positions[2], -1.0f, 1.0f);

    ImGui::SeparatorText("Vertex Colors");
    ImGui::ColorEdit3("Vertex 0", state.colors[0]);
    ImGui::ColorEdit3("Vertex 1", state.colors[1]);
    ImGui::ColorEdit3("Vertex 2", state.colors[2]);

    ImGui::SeparatorText("Fragment");
    ImGui::ColorEdit3("Global tint", state.tint);

    ImGui::Spacing();
    if (ImGui::Button("Reset to defaults")) reset_triangle();

    ImGui::SeparatorText("Info");
    ImGui::Text("Time  %.2f s", state.ticks);
    ImGui::Text("FPS   %.1f  (%.2f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);

    ImGui::End();
    ImGui::Render();

    // 2. Acquire command buffer and swapchain — one per frame, shared by all passes
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state.gpu.device);
    if (!cmd) return false;

    SDL_GPUTexture *swapchain = NULL;
    Uint32 sw = 0, sh = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, state.win.window, &swapchain, &sw, &sh)) {
        SDL_CancelGPUCommandBuffer(cmd);
        return false;
    }
    if (!swapchain) {
        SDL_SubmitGPUCommandBuffer(cmd);
        return true;
    }

    // 3. Upload ImGui vertex/index buffers via a copy pass BEFORE any render pass.
    //    This is MANDATORY for imgui_impl_sdlgpu3 — copy operations are not
    //    allowed inside a render pass.
    ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), cmd);

    // 4. Pass 1: scene (CLEAR to erase previous frame)
    {
        SDL_GPUColorTargetInfo t = {};
        t.texture     = swapchain;
        t.load_op     = SDL_GPU_LOADOP_CLEAR;
        t.store_op    = SDL_GPU_STOREOP_STORE;
        t.clear_color = {state.gpu.clear_r, state.gpu.clear_g, state.gpu.clear_b, state.gpu.clear_a};

        SDL_PushGPUVertexUniformData(cmd, 0, &v, (Uint32)sizeof(v));
        SDL_PushGPUFragmentUniformData(cmd, 0, &f, (Uint32)sizeof(f));

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &t, 1, NULL);
        SDL_BindGPUGraphicsPipeline(pass, state.pipeline);
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(pass);
    }

    // 5. Pass 2: ImGui (LOAD to composite on top of the scene)
    {
        SDL_GPUColorTargetInfo t = {};
        t.texture  = swapchain;
        t.load_op  = SDL_GPU_LOADOP_LOAD;
        t.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &t, 1, NULL);
        ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), cmd, pass);
        SDL_EndGPURenderPass(pass);
    }

    return SDL_SubmitGPUCommandBuffer(cmd);
}

int main()
{
    windowInit(&state.win);
    state.win.width  = WIDTH;
    state.win.height = HEIGHT;
    state.win.title  = "triangle test";

    ASSERT(createWindow(&state.win));

    cameraInit(&state.cam);
    state.cam.position = vec3(0.0f, 3.0f, 10.0f);
    state.cam.yaw      = -90.0f;
    state.cam.pitch    = -20.0f;
    state.cam.fov      = 75.0f;
    cameraUpdate(&state.cam);

    inputInit(&state.input);

    // gpuInit destroys win.renderer — ImGui must be init'd after this
    ASSERT(gpuInit(&state.gpu, &state.win));

    state.pipeline = gpuCreatePipeline(&state.gpu, "basic_triangle", 1, 1);
    ASSERT(state.pipeline);

    reset_triangle();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplSDL3_InitForSDLGPU(state.win.window);

    ImGui_ImplSDLGPU3_InitInfo gpu3_info = {};
    gpu3_info.Device            = state.gpu.device;
    gpu3_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(state.gpu.device, state.win.window);
    gpu3_info.MSAASamples       = SDL_GPU_SAMPLECOUNT_1;
    ImGui_ImplSDLGPU3_Init(&gpu3_info);

    state.running           = true;
    state.move_speed        = 0.1f;
    state.mouse_sensitivity = 0.3f;

    while (state.running) {
        update();
        state.ticks = (float)SDL_GetTicks() * 0.001f;
        if (!render()) state.running = false;
        updateFrame(&state.win);
    }

    cleanup();
    return 0;
}