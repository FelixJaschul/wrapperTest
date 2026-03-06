#include <cstdio>

#define CORE_IMPLEMENTATION
#define SDL_IMPLEMENTATION
#define IMGUI_IMPLEMENTATION
#define GPU_IMPLEMENTATION
#define KEYS_IMPLEMENTATION
#include "core.h"

int main()
{
    Window_t win;

    Input input;

    windowInit(&win);
    win.width = 800;
    win.height = 600;
    win.title = "GPU Triangle";
    ASSERT(createWindow(&win));

    inputInit(&input);

    Gpu gpu;
    ASSERT(gpuInit(&gpu, &win));
    gpuSetClearColor(&gpu, 0, 0, 0, 1);

    SDL_GPUGraphicsPipeline *pipeline = gpuCreatePipeline(&gpu, "basic_triangle", 0, 0);
    ASSERT(pipeline);

    imguiInit(&win, gpu.device, SDL_GetGPUSwapchainTextureFormat(gpu.device, win.window));

    bool running = true;
    while (running)
    {
        pollEvents(&win, &input);
        if (isKeyDown(&input, KEY_ESCAPE)) running = false;

        imguiNewFrame();
        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::Begin("STATE", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs);
        ImGui::Text("FPS: %.1f  |  Time: %.2fs", ImGui::GetIO().Framerate, (float)SDL_GetTicks() * 0.001f);
        ImGui::End();
        ImGui::Render();

        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gpu.device);
        SDL_GPUTexture *swapchain = NULL;
        Uint32 sw, sh;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, win.window, &swapchain, &sw, &sh)) { SDL_CancelGPUCommandBuffer(cmd); continue; }
        if (!swapchain) { SDL_SubmitGPUCommandBuffer(cmd); continue; }

        imguiPrepareDrawData(cmd);

        // Triangle pass
        {
            SDL_GPUColorTargetInfo t = {};
            t.texture = swapchain;
            t.load_op = SDL_GPU_LOADOP_CLEAR;
            t.store_op = SDL_GPU_STOREOP_STORE;
            SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &t, 1, NULL);
            SDL_BindGPUGraphicsPipeline(pass, pipeline);
            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            SDL_EndGPURenderPass(pass);
        }

        // ImGui pass
        {
            SDL_GPUColorTargetInfo t = {};
            t.texture = swapchain;
            t.load_op = SDL_GPU_LOADOP_LOAD;
            t.store_op = SDL_GPU_STOREOP_STORE;
            SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &t, 1, NULL);
            imguiRenderDrawData(cmd, pass);
            SDL_EndGPURenderPass(pass);
        }

        SDL_SubmitGPUCommandBuffer(cmd);
        updateFrame(&win);
    }

    gpuReleasePipeline(&gpu, &pipeline);
    gpuFree(&gpu);
    destroyWindow(&win);
    return 0;
}
