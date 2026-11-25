#include "window.h"
#include <array>
#include <algorithm>
#ifdef CYC_WITH_RECOMP_UI
#include "cyc_ui.h"
extern "C" void smb3_ui_prepare_fonts(void);
extern "C" int smb3_ui_event(const SDL_Event *event);
extern "C" int smb3_ui_owns_input(void);
extern "C" int smb3_ui_receives_events(void);
#endif

namespace {
bool fullscreen_pressed = false;
#ifdef CYC_WITH_RECOMP_UI
SDL_FRect picture_bounds{};
#endif
}

#ifdef CYC_WITH_RECOMP_UI
extern "C" SDL_FRect smb3_picture_bounds(void) { return picture_bounds; }

extern "C" void smb3_process_ui_event(const SDL_Event *event) {
    const bool input = (event->type >= SDL_KEYDOWN && event->type <= SDL_KEYMAPCHANGED) ||
                       (event->type >= SDL_MOUSEMOTION && event->type <= SDL_MOUSEWHEEL);
    // Gameplay events must not queue up and navigate the next menu when ImGui resumes.
    if (!input || smb3_ui_receives_events()) cyc_ui_process_event(event);
}
#endif

extern "C" int SDLCALL smb3_poll_window_event(SDL_Event *event) {
    int result;
#ifdef CYC_WITH_RECOMP_UI
    smb3_ui_prepare_fonts();
    do { result = SDL_PollEvent(event); } while (result && event && smb3_ui_event(event));
#else
    result = SDL_PollEvent(event);
#endif
    if (result && event && event->type == SDL_KEYDOWN && !event->key.repeat &&
        event->key.keysym.scancode == SDL_SCANCODE_F11) fullscreen_pressed = true;
    return result;
}

extern "C" const Uint8 *SDLCALL smb3_keyboard_state(int *count) {
    int size = 0;
    const Uint8 *physical = SDL_GetKeyboardState(&size);
    static std::array<Uint8, SDL_NUM_SCANCODES> keys{};
    keys.fill(0);
    if (physical) std::copy_n(physical, std::min<int>(size, keys.size()), keys.begin());
#ifdef CYC_WITH_RECOMP_UI
    if (smb3_ui_owns_input()) keys[SDL_SCANCODE_ESCAPE] = 0;
#endif
    if (fullscreen_pressed) keys[SDL_SCANCODE_F11] = 1;
    if (count) *count = static_cast<int>(keys.size());
    return keys.data();
}

extern "C" int SDLCALL smb3_copy_picture(SDL_Renderer *renderer, SDL_Texture *texture,
                                        const SDL_Rect *source, const SDL_Rect *destination) {
    fullscreen_pressed = false; // Keep a quick F11 tap until this frame has sampled shortcuts.
    // The runtime menu can restore a viewport from before the picture changed width.
    int width = 0, height = 0;
    SDL_RenderGetLogicalSize(renderer, &width, &height);
    if (SDL_RenderSetLogicalSize(renderer, 0, 0) < 0 ||
        SDL_RenderSetViewport(renderer, nullptr) < 0 ||
        SDL_RenderSetLogicalSize(renderer, width, height) < 0) return -1;
#ifdef CYC_WITH_RECOMP_UI
    int left, top, right, bottom;
    SDL_RenderLogicalToWindow(renderer, 0, 0, &left, &top);
    SDL_RenderLogicalToWindow(renderer, width, height, &right, &bottom);
    picture_bounds = {float(left), float(top), float(right - left), float(bottom - top)};
#endif
    return SDL_RenderCopy(renderer, texture, source, destination);
}
