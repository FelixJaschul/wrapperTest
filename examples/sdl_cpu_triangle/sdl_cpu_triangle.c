#include <cstdio>

#define CORE_IMPLEMENTATION
#define MATH_IMPLEMENTATION
#define KEYS_IMPLEMENTATION
#define CAMERA_IMPLEMENTATION
#define SDL_IMPLEMENTATION
#define IMGUI_IMPLEMENTATION
#include "core.h"

#define WIDTH 1270
#define HEIGHT 850

typedef struct {
    Window_t win;
    Camera cam;
    Input input;

    bool running;
    float ticks;
    float move_speed;
    float mouse_sensitivity;

    float positions[3][2];
    float colors[3][3];
    float tint[3];
} state_t;

static state_t state = {0};
static SDL_Texture *texture = NULL;

#define cleanup() do { \
    if (texture) SDL_DestroyTexture(texture); \
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

static void draw_triangle_filled(float x0, float y0, float x1, float y1, float x2, float y2, uint32_t color)
{
    // Sort vertices by y-coordinate
    if (y0 > y1) { float tx = x0; x0 = x1; x1 = tx; float ty = y0; y0 = y1; y1 = ty; }
    if (y1 > y2) { float tx = x1; x1 = x2; x2 = tx; float ty = y1; y1 = y2; y2 = ty; }
    if (y0 > y1) { float tx = x0; x0 = x1; x1 = tx; float ty = y0; y0 = y1; y1 = ty; }

    int y0i = (int)y0, y1i = (int)y1, y2i = (int)y2;

    for (int y = y0i; y <= y2i; y++) {
        float x_start, x_end;

        if (y < y1i) {
            if (y1i == y0i) continue;
            float t1 = (float)(y - y0i) / (float)(y1i - y0i);
            float t2 = (float)(y - y0i) / (float)(y2i - y0i);
            x_start = x0 + (x1 - x0) * t1;
            x_end   = x0 + (x2 - x0) * t2;
        } else {
            if (y2i == y1i) continue;
            float t1 = (float)(y - y1i) / (float)(y2i - y1i);
            float t2 = (float)(y - y0i) / (float)(y2i - y0i);
            x_start = x1 + (x2 - x1) * t1;
            x_end   = x0 + (x2 - x0) * t2;
        }

        if (x_start > x_end) { float tmp = x_start; x_start = x_end; x_end = tmp; }

        int x0i = (int)x_start;
        int x1i = (int)x_end;

        for (int x = x0i; x <= x1i; x++) {
            if (x >= 0 && x < state.win.bWidth && y >= 0 && y < state.win.bHeight) {
                drawPixel(&state.win, x, y, color);
            }
        }
    }
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
    // Clear buffer - ABGR format: dark background
    for (int i = 0; i < state.win.bWidth * state.win.bHeight; i++) {
        state.win.buffer[i] = 0xFF241E1A; // Dark background (ABGR: A=255, B=36, G=30, R=26)
    }

    // Convert NDC coordinates to screen space
    float sx0 = (state.positions[0][0] * 0.5f + 0.5f) * state.win.bWidth;
    float sy0 = (1.0f - (state.positions[0][1] * 0.5f + 0.5f)) * state.win.bHeight;
    float sx1 = (state.positions[1][0] * 0.5f + 0.5f) * state.win.bWidth;
    float sy1 = (1.0f - (state.positions[1][1] * 0.5f + 0.5f)) * state.win.bHeight;
    float sx2 = (state.positions[2][0] * 0.5f + 0.5f) * state.win.bWidth;
    float sy2 = (1.0f - (state.positions[2][1] * 0.5f + 0.5f)) * state.win.bHeight;

    // Apply tint to colors - ABGR format for SDL_PIXELFORMAT_ABGR8888
    uint32_t c0 = 0xFF000000 |
        ((uint32_t)(state.colors[0][2] * state.tint[2] * 255.0f) << 16) |
        ((uint32_t)(state.colors[0][1] * state.tint[1] * 255.0f) << 8) |
        ((uint32_t)(state.colors[0][0] * state.tint[0] * 255.0f));
    uint32_t c1 = 0xFF000000 |
        ((uint32_t)(state.colors[1][2] * state.tint[2] * 255.0f) << 16) |
        ((uint32_t)(state.colors[1][1] * state.tint[1] * 255.0f) << 8) |
        ((uint32_t)(state.colors[1][0] * state.tint[0] * 255.0f));
    uint32_t c2 = 0xFF000000 |
        ((uint32_t)(state.colors[2][2] * state.tint[2] * 255.0f) << 16) |
        ((uint32_t)(state.colors[2][1] * state.tint[1] * 255.0f) << 8) |
        ((uint32_t)(state.colors[2][0] * state.tint[0] * 255.0f));

    // Draw triangle (simple flat shading with vertex colors)
    // For simplicity, draw with average color - ABGR format
    uint32_t avg_color = 0xFF000000 |
        ((uint32_t)(((state.colors[0][2] + state.colors[1][2] + state.colors[2][2]) / 3.0f) * state.tint[2] * 255.0f) << 16) |
        ((uint32_t)(((state.colors[0][1] + state.colors[1][1] + state.colors[2][1]) / 3.0f) * state.tint[1] * 255.0f) << 8) |
        ((uint32_t)(((state.colors[0][0] + state.colors[1][0] + state.colors[2][0]) / 3.0f) * state.tint[0] * 255.0f));

    draw_triangle_filled(sx0, sy0, sx1, sy1, sx2, sy2, avg_color);

    // Update framebuffer texture
    if (!texture) {
        texture = SDL_CreateTexture(state.win.renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, state.win.bWidth, state.win.bHeight);
        if (!texture) {
            fprintf(stderr, "Failed to create texture: %s\n", SDL_GetError());
            return false;
        }
    }

    if (!updateFramebuffer(&state.win, texture)) {
        fprintf(stderr, "Failed to update framebuffer: %s\n", SDL_GetError());
        return false;
    }

    // Build ImGui UI
    imguiNewFrame();

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
    if (ImGui::Button("Reset to defaults"))
        reset_triangle();

    ImGui::SeparatorText("Info");
    ImGui::Text("Time  %.2f s", state.ticks);
    ImGui::Text("FPS   %.1f  (%.2f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);

    ImGui::End();
    imguiEndFrame(&state.win);

    SDL_RenderPresent(state.win.renderer);
    return true;
}

int main()
{
    windowInit(&state.win);
    state.win.width  = WIDTH;
    state.win.height = HEIGHT;
    state.win.title  = "triangle test (SDL_Renderer)";

    ASSERT(createWindow(&state.win));

    cameraInit(&state.cam);
    state.cam.position = vec3(0.0f, 3.0f, 10.0f);
    state.cam.yaw      = -90.0f;
    state.cam.pitch    = -20.0f;
    state.cam.fov      = 75.0f;
    cameraUpdate(&state.cam);

    inputInit(&state.input);

    // ImGui is already initialized in createWindow() when GPU_IMPLEMENTATION is not defined
    // But we can re-init if needed (it's safe to skip since createWindow() already did it)

    reset_triangle();

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
