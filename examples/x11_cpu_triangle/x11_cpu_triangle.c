#include <cstdio>

#define CORE_IMPLEMENTATION
#define KEYS_IMPLEMENTATION
#include "core.h"

int main()
{
    Window_t win;

    Input input;

    windowInit(&win);
    win.width = 800;
    win.height = 600;
    win.title = "X11 CPU Triangle";
    ASSERT(createWindow(&win));

    inputInit(&input);

    bool running = true;
    while (running)
    {
        pollEvents(&win, &input);
        if (isKeyDown(&input, KEY_ESCAPE)) running = false;

        for (int i = 0; i < win.bWidth * win.bHeight; i++) win.buffer[i] = 0xFF000000;

        for (int y = 0; y < win.bHeight; y++)
        {
            for (int x = 0; x < win.bWidth; x++)
            {
                const float u = (float)x / win.bWidth * 2 - 1;
                const float v = (1 - (float)y / win.bHeight) * 2 - 1;

                const float ax = 0.0f,  ay = 0.5f, bx = -0.5f, by = -0.5f, cx = 0.5f,  cy = -0.5f;
                const float e0 = (bx - ax) * (v - ay) - (by - ay) * (u - ax);
                const float e1 = (cx - bx) * (v - by) - (cy - by) * (u - bx);
                const float e2 = (ax - cx) * (v - cy) - (ay - cy) * (u - cx);

                if (e0 >= 0 && e1 >= 0 && e2 >= 0) win.buffer[y * win.bWidth + x] = 0xFF800080;
            }
        }

        updateFramebuffer(&win);
        updateFrame(&win);
    }

    destroyWindow(&win);
    return 0;
}
