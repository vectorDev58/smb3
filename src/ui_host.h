#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int smb3_ui_source(int player, int replacement);
int smb3_ui_bind(int player, int button, int gamepad, int replacement);
void smb3_ui_bind_label(int player, int button, int gamepad, char *out, size_t size);
void smb3_ui_save(void);
// Controllers in Controls: count, names, the pad a player reads now, the pad chosen for a
// player (-1: automatic), and choosing one (-1 returns to automatic).
int smb3_ui_pads(void);
void smb3_ui_pad_name(int pad, char *out, size_t size);
int smb3_ui_pad_in_use(int player);
int smb3_ui_chosen_pad(int player);
void smb3_ui_choose_pad(int player, int pad);
bool smb3_coop_enabled(void); // src/remaster.cpp
void smb3_coop_join(void); // src/remaster.cpp
const char *smb3_ui_status(void);
bool smb3_ui_states_available(void);
bool smb3_ui_restore(const uint8_t *data, size_t size, char *error, size_t capacity);
#ifdef __cplusplus
}
#endif
