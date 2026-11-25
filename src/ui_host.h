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
const char *smb3_ui_status(void);
bool smb3_ui_states_available(void);
bool smb3_ui_restore(const uint8_t *data, size_t size, char *error, size_t capacity);
#ifdef __cplusplus
}
#endif
