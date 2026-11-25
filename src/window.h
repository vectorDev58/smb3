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
#ifdef __cplusplus
}
#endif

#ifdef SMB3_HOST_COPY
#define SDL_RenderCopy smb3_copy_picture
#define SDL_PollEvent smb3_poll_window_event
#define SDL_GetKeyboardState smb3_keyboard_state
#define cyc_ui_menu_open smb3_host_menu_open
#define cyc_ui_toggle_menu smb3_toggle_host_menu
#define cyc_ui_process_event smb3_process_ui_event
#endif
