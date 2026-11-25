#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
bool smb3_ui_spanish(void);
#ifdef __cplusplus
}
#endif

static inline const char *smb3_text(const char *english, const char *spanish) {
    return smb3_ui_spanish() ? spanish : english;
}
