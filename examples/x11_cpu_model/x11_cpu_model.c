#include <cstdio>

#define CORE_IMPLEMENTATION
#define MATH_IMPLEMENTATION
#define KEYS_IMPLEMENTATION
#define CAMERA_IMPLEMENTATION
#define MODEL_IMPLEMENTATION
#define RENDER3D_IMPLEMENTATION
#include "core.h"

#define WIDTH 800
#define HEIGHT 600
#define MAX_MODELS 16

typedef struct {
    Window_t win;
    Camera cam;
    Input input;
    Renderer renderer;
    Model models[MAX_MODELS];
    int num_models;
    bool running;
    float rotation;
} state_t;

static state_t state = {};

static void update()
{
    if (pollEvents(&state.win, &state.input)) {
        state.running = false;
        return;
    }

    if (isKeyDown(&state.input, KEY_ESCAPE)) { state.running = false; return; }

    if (isKeyDown(&state.input, KEY_LSHIFT)) releaseMouse(state.win.display, state.win.window, &state.input);
    else if (!isMouseGrabbed(&state.input)) grabMouse(state.win.display, state.win.window, state.win.width, state.win.height, &state.input);

    int dx, dy;
    getMouseDelta(&state.input, &dx, &dy);
    if (isMouseGrabbed(&state.input)) cameraRotate(&state.cam, dx * 0.3f, -dy * 0.3f);

    if (isKeyDown(&state.input, KEY_W)) cameraMove(&state.cam, state.cam.front, 0.05f);
    if (isKeyDown(&state.input, KEY_S)) cameraMove(&state.cam, mul(state.cam.front, -1.0f), 0.05f);
    if (isKeyDown(&state.input, KEY_A)) cameraMove(&state.cam, mul(state.cam.right, -1.0f), 0.05f);
    if (isKeyDown(&state.input, KEY_D)) cameraMove(&state.cam, state.cam.right, 0.05f);

    // Rotate model
    state.rotation += 0.01f;
    for (int i = 0; i < state.num_models; i++) {
        modelTransform(&state.models[i],
            state.models[i].position,
            vec3(0.0f, state.rotation, 0.0f),
            state.models[i].scale);
    }
    modelUpdate(state.models, state.num_models);
}

static void render()
{
    renderClear(&state.renderer);
    renderScene(&state.renderer, state.models, state.num_models);
    updateFramebuffer(&state.win);
}

int main()
{
    windowInit(&state.win);
    state.win.width = WIDTH;
    state.win.height = HEIGHT;
    state.win.title = "X11 CPU Model";

    ASSERT(createWindow(&state.win));

    cameraInit(&state.cam);
    state.cam.position = vec3(1.0f, 1.4f, -4.7f);
    state.cam.yaw = 102.0f;
    state.cam.pitch = -17.0f;
    state.cam.fov = 60.0f;
    cameraUpdate(&state.cam);

    inputInit(&state.input);

    renderInit(&state.renderer, &state.win, &state.cam);
    state.renderer.light_dir = norm(vec3(0.5f, -1.0f, 0.5f));

    Model* cube = modelCreate(state.models, &state.num_models, MAX_MODELS, vec3(0.8f, 0.4f, 0.2f), 0.0f, 0.5f);
    modelLoad(cube, "examples/x11_cpu_model/cube.obj");
    modelTransform(cube, vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 0.0f), vec3(2.0f, 2.0f, 2.0f));
    modelUpdate(state.models, state.num_models);

    state.running = true;
    state.rotation = 0.0f;

    while (state.running) {
        printf("MODEL: %f | %f | %f\n", cube->position.x, cube->position.y, cube->position.z);
        printf("CAMXY: %f | %f | %f\n", state.cam.position.x, state.cam.position.y, state.cam.position.z);
        printf("CAMYP: %f | %f\n", state.cam.pitch, state.cam.yaw);
        update();
        render();
        updateFrame(&state.win);
    }

    for (int i = 0; i < state.num_models; i++) modelFree(&state.models[i]);
    renderFree(&state.renderer);
    destroyWindow(&state.win);
    return 0;
}