#include <imgui.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#define CORE_IMPLEMENTATION
#define MATH_IMPLEMENTATION
#define KEYS_IMPLEMENTATION
#define CAMERA_IMPLEMENTATION
#define SDL_IMPLEMENTATION
#define IMGUI_IMPLEMENTATION
#include "core.h"

#define WIDTH 800
#define HEIGHT 600
#define RENDER_SCALE 0.5f

typedef struct {
    Window_t win;
    SDL_Texture* texture;

    Camera cam;
    Input input;

    bool running;
    float delta;
    float ticks;
    float radius;
    float move_speed;
    float mouse_sensitivity;
} state_t;

state_t state = {0};

#define cleanup() do { \
    if (state.texture) SDL_DestroyTexture(state.texture); \
    destroyWindow(&state.win); \
} while(0)

static Vec3 trace_ray(const Ray* ray)
{
    const Vec3 oc = ray->origin;
    const float b = dot(oc, ray->direction);
    const float d = b*b - dot(oc, oc) + state.radius * state.radius;

    if (d < 0.0f) return vec3(0,0,0);
    const float t = -b - sqrtf(d);
    if (t <= 0.00001f) return vec3(0,0,0);

    return norm(add(ray->origin, mul(ray->direction, t)));
}

static void render_frame()
{
    const float vp_height = 2.0f * tanf((float)(state.cam.fov * M_PI / 180.0f) / 2.0f);
    const float vp_width = vp_height * (float)state.win.bWidth / (float)state.win.bHeight;

    for (int y = 0; y < state.win.bHeight; y++)
    {
        for (int x = 0; x < state.win.bWidth; x++)
        {
            const float u = ((float)x / (float)(state.win.bWidth - 1) - 0.5f) * vp_width;
            const float v = (0.5f - (float)y / (float)(state.win.bHeight - 1)) * vp_height;

            const Ray ray = cameraGetRay(&state.cam, u, v);
            const Vec3 c = trace_ray(&ray);
            
            uint8_t r, g, b;
            if (c.x != 0.0f || c.y != 0.0f || c.z != 0.0f) {
                float phase = atan2f(c.z, c.x) + state.ticks * 0.002f;
                r = (uint8_t)((0.5f + 0.5f * sinf(phase)) * 255.0f);
                g = (uint8_t)((0.5f + 0.5f * sinf(phase + 2.0943951f)) * 255.0f);
                b = (uint8_t)((0.5f + 0.5f * sinf(phase + 4.1887902f)) * 255.0f);
            } else r = g = b = 0;

            state.win.buffer[y * state.win.bWidth + x] = (0xFF<<24)|(r<<16)|(g<<8)|b;
        }
    }
}

static void update()
{
    if (pollEvents(&state.win, &state.input)) {
        state.running = false;
        return;
    }

    // Mouse grab control
    if (isKeyDown(&state.input, KEY_LSHIFT)) releaseMouse(state.win.window, &state.input);
    else if (!isMouseGrabbed(&state.input)) grabMouse(state.win.window, state.win.width, state.win.height, &state.input);

    // Camera rotation
    int dx, dy;
    getMouseDelta(&state.input, &dx, &dy);
    cameraRotate(&state.cam, (float)dx * state.mouse_sensitivity, (float)(-dy) * state.mouse_sensitivity);

    // Camera movement
    if (isKeyDown(&state.input, KEY_W)) cameraMove(&state.cam, state.cam.front, state.move_speed);
    if (isKeyDown(&state.input, KEY_S)) cameraMove(&state.cam, mul(state.cam.front,-1), state.move_speed);
    if (isKeyDown(&state.input, KEY_A)) cameraMove(&state.cam, mul(state.cam.right,-1), state.move_speed);
    if (isKeyDown(&state.input, KEY_D)) cameraMove(&state.cam, state.cam.right, state.move_speed);
}

int main() 
{
    windowInit(&state.win);
    state.win.width   = WIDTH;
    state.win.height  = HEIGHT;
    state.win.bWidth  = (int)(WIDTH  * RENDER_SCALE);
    state.win.bHeight = (int)(HEIGHT * RENDER_SCALE);
    state.win.title = "ray";

    ASSERT(createWindow(&state.win));

    state.texture = SDL_CreateTexture(
        state.win.renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        state.win.bWidth,
        state.win.bHeight
    );

    ASSERT(state.texture);

    cameraInit(&state.cam);
    state.cam.position = vec3(0,3,10);
    state.cam.yaw = -90;
    state.cam.pitch = -20;
    cameraUpdate(&state.cam);

    inputInit(&state.input);

    state.running = true;
    state.move_speed = 0.1f;
    state.mouse_sensitivity = 0.3f;
    state.radius = 1.0f;

    while (state.running)
    {
        update();

        render_frame();
        ASSERT(updateFramebuffer(&state.win, state.texture));
        state.delta = getDelta(&state.win);
        state.ticks = SDL_GetTicks();

        imguiNewFrame(); // this needs cpp because im toooo lazy to implement cimgui
            ImGui::Begin("status");
            ImGui::Text("Camera pos: %.2f, %.2f, %.2f", state.cam.position.x, state.cam.position.y, state.cam.position.z);
            ImGui::Text("Fps: %.2f", getFPS(&state.win));
            ImGui::Text("Delta: %.4f ms", getDelta(&state.win) * 1000.0);
            ImGui::Text("Resolution: %dx%d (buffer: %dx%d)", state.win.width, state.win.height, state.win.bWidth, state.win.bHeight);
            ImGui::End();
        imguiEndFrame(&state.win);

        SDL_RenderPresent(state.win.renderer);
        updateFrame(&state.win);
    }

    // Cleanup
    cleanup();
    return 0;
}
