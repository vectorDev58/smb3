#pragma once
#include <SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
int SDLCALL smb3_copy_picture(SDL_Renderer *renderer, SDL_Texture *texture,
                             const SDL_Rect *source, const SDL_Rect *destination);
int SDLCALL smb3_poll_window_event(SDL_Event *event);
const Uint8 *SDLCALL smb3_keyboard_state(int *count);
bool smb3_host_menu_open(void);
void smb3_toggle_host_menu(void);
SDL_FRect smb3_picture_bounds(void);
void smb3_process_ui_event(const SDL_Event *event);
struct CycUiHost;
bool smb3_ui_init(SDL_Window *window, SDL_Renderer *renderer, const struct CycUiHost *host);
double smb3_frame_seconds(void);
bool smb3_audio_enable(int sample_rate);
#ifdef __cplusplus
}
#endif

// The frame rate that keeps pace with a VSync display: the display's refresh, or a whole fraction of
// it, when that is within 1% of the machine's 60.0988 Hz; otherwise 0, and the machine keeps its own.
static inline double smb3_display_frame_rate(int refresh_hz, double machine_hz) {
    const int per_frame = (int)(refresh_hz / machine_hz + 0.5);
    const double rate = per_frame > 0 ? (double)refresh_hz / per_frame : 0;
    return rate > machine_hz * 0.99 && rate < machine_hz * 1.01 ? rate : 0;
}

#ifdef SMB3_HOST_COPY
#define SDL_RenderCopy smb3_copy_picture
#define SDL_PollEvent smb3_poll_window_event
#define SDL_GetKeyboardState smb3_keyboard_state
#define cyc_ui_menu_open smb3_host_menu_open
#define cyc_ui_toggle_menu smb3_toggle_host_menu
#define cyc_ui_process_event smb3_process_ui_event
#ifdef CYC_WITH_RECOMP_UI
#define cyc_ui_init smb3_ui_init
#define cyc_host_frame_seconds smb3_frame_seconds
#define cyc_audio_enable smb3_audio_enable
#endif
#endif
