#pragma once
#include "imgui.h"

namespace smb3_state_style {
using namespace ImGui;
bool Begin(const char *title, bool *open, ImGuiWindowFlags flags);
void PushFont(ImFont *font);
void Text(const char *format, ...) IM_FMTARGS(1);
void TextWrapped(const char *format, ...) IM_FMTARGS(1);
void TextUnformatted(const char *text);
bool Button(const char *label, const ImVec2 &size);
bool Selectable(const char *label, bool selected, ImGuiSelectableFlags flags, const ImVec2 &size);
bool SliderInt(const char *label, int *value, int minimum, int maximum, const char *format);
}

#ifdef SMB3_STATE_MENU_STYLE
#define ImGui smb3_state_style
#endif
