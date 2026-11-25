#define SDL_MAIN_HANDLED
#include "window.h"
#include <algorithm>
#include <cassert>
#include <vector>

int main() {
    assert(SDL_Init(SDL_INIT_EVENTS) == 0);
    SDL_Event tap{};
    tap.type = SDL_KEYDOWN;
    tap.key.keysym.scancode = SDL_SCANCODE_F11;
    assert(SDL_PushEvent(&tap) == 1);
    tap.type = SDL_KEYUP;
    assert(SDL_PushEvent(&tap) == 1);
    while (smb3_poll_window_event(&tap)) {}
    int keys = 0;
    assert(smb3_keyboard_state(&keys)[SDL_SCANCODE_F11]);
    for (const auto size : {SDL_Point{1280, 720}, SDL_Point{1024, 768}, SDL_Point{1680, 720}}) {
        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, size.x, size.y, 32, SDL_PIXELFORMAT_ARGB8888);
        assert(surface);
        SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
        assert(renderer);
        for (const int width : {256, 320, 426, 560, 854}) {
            SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                SDL_TEXTUREACCESS_STATIC, width, 240);
            assert(texture);
            std::vector<uint32_t> pixels(width * 240, 0xff37c980);
            assert(SDL_UpdateTexture(texture, nullptr, pixels.data(), width * 4) == 0);
            assert(SDL_RenderSetLogicalSize(renderer, width, 240) == 0);
            // Reproduce a menu restoring the old native viewport after a width change.
            SDL_Rect stale{0, 0, 256, 240};
            assert(SDL_RenderSetViewport(renderer, &stale) == 0);
            assert(SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) == 0);
            assert(SDL_RenderClear(renderer) == 0);
            SDL_Rect destination{0, 0, width, 240};
            assert(smb3_copy_picture(renderer, texture, nullptr, &destination) == 0);
            assert(!smb3_keyboard_state(&keys)[SDL_SCANCODE_F11]);
            assert(SDL_RenderSetLogicalSize(renderer, 0, 0) == 0);
            assert(SDL_RenderSetViewport(renderer, nullptr) == 0);
            std::vector<uint32_t> actual(size.x * size.y);
            assert(SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888,
                actual.data(), size.x * 4) == 0);
            const double scale = std::min(double(size.x) / width, double(size.y) / 240);
            const int left = int((size.x - width * scale) / 2);
            const int right = int((size.x + width * scale) / 2);
            const int top = int((size.y - 240 * scale) / 2);
            const int bottom = int((size.y + 240 * scale) / 2);
            for (int y : {top + 3, size.y / 2, bottom - 3})
                for (int x : {left + 3, size.x / 2, right - 3})
                    assert(actual[y * size.x + x] == 0xff37c980);
            if (left > 3) assert(actual[(size.y / 2) * size.x + left - 3] == 0xff000000);
            if (top > 3) assert(actual[(top - 3) * size.x + size.x / 2] == 0xff000000);
            SDL_DestroyTexture(texture);
        }
        SDL_DestroyRenderer(renderer);
        SDL_FreeSurface(surface);
    }
    SDL_Quit();
}
