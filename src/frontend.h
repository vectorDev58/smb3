#pragma once

ImFont *ui_regular = nullptr;
smb3::MenuArt ui_art;
bool ui_fonts_ready = false, ui_texture_ready = false, ui_focus = true;
int main_choice = 0, pause_choice = 0;
const char *character_error = "";
bool controller_hints = false;
bool ui_mouse = false; // The last pointer motion or click is newer than any key or button.
unsigned consumed_controller_buttons = 0;

void frontend_pause() {
    pause_choice = 0;
    hold_pause();
    frontend_screen(smb3::Screen::Pause);
}

void frontend_resume() {
    release_pause();
    frontend_screen(smb3::Screen::Playing);
}
int binding_player = 0, binding_button = -1, binding_pad = 0, controls_player = 0;
int controls_row = 2, controls_column = 0;
int practice_row = 0, practice_power = 0, practice_world = 0, practice_level = 0;
std::string binding_status;
struct CharacterChoice {
    smb3::Screen back = smb3::Screen::Main;
    bool start_campaign = false, new_game = false, load_save = false;
    int index = 0, players = 1;
} character_choice;

void frontend_screen(smb3::Screen screen) {
    if (ImGui::GetCurrentContext() && (frontend.screen == smb3::Screen::Playing || frontend.screen == smb3::Screen::Intro)) {
        ImGui::GetIO().ClearInputKeys();
        ImGui::GetIO().ClearInputMouse();
        ImGui::GetIO().ClearEventsQueue();
    }
    frontend.screen = screen;
    ui_focus = true;
    if (screen == smb3::Screen::Main) {
        main_choice = 0;
        frontend.settings_return = smb3::Screen::Main;
        cyc_render_frame_done();
    }
    if (screen == smb3::Screen::Controls) {
        controls_row = 2;
        controls_column = controller_hints ? 1 : 0;
        binding_status.clear();
    }
    if (screen == smb3::Screen::Practice) {
        practice_row = practice_in_level() ? 0 : 2;
        practice_error = "";
    }
}
void frontend_intro() {
    frontend.intro_tick = 0;
    frontend.skip_intro = false;
    frontend_screen(smb3::Screen::Intro);
    cyc_ui_set_toast(nullptr, nullptr);
}
void frontend_settings(smb3::Screen back) {
    frontend.settings_return = back;
    frontend_screen(smb3::Screen::Settings);
    recomp_runtime_ui_open(cyc_ui_runtime());
    RecompRuntimeUiRowView first{};
    if (recomp_runtime_ui_view_row(cyc_ui_runtime(), 0, &first) && first.item)
        recomp_runtime_ui_handle_input(cyc_ui_runtime(), RECOMP_RUNTIME_UI_INPUT_BACK, 1, 0);
    const size_t selected = recomp_runtime_ui_view_selected_row(cyc_ui_runtime());
    for (size_t i = 0; i < selected; ++i)
        recomp_runtime_ui_handle_input(cyc_ui_runtime(), RECOMP_RUNTIME_UI_INPUT_UP, 1, 0);
}
void frontend_characters(smb3::Screen back, bool start_campaign, bool new_game) {
    character_choice = {back, start_campaign, new_game, start_campaign && !new_game && !frontend.started &&
                        campaign_save_world >= 0, character, frontend.players};
    character_error = "";
    frontend_screen(smb3::Screen::Characters);
}
void close_character_choice() {
    if (character_choice.back == smb3::Screen::Settings && !recomp_runtime_ui_is_open(cyc_ui_runtime()))
        recomp_runtime_ui_open_section(cyc_ui_runtime(), "Remastered");
    frontend_screen(character_choice.back);
}
void frontend_back() {
    using smb3::Screen;
    if (binding_button >= 0) { binding_button = -1; binding_status.clear(); return; }
    if (frontend.screen == Screen::Controls || frontend.screen == Screen::Practice) frontend_screen(Screen::Settings);
    else if (frontend.screen == Screen::Settings) {
        auto *model = cyc_ui_runtime();
        recomp_runtime_ui_handle_input(model, RECOMP_RUNTIME_UI_INPUT_BACK, 1, 0);
        if (!recomp_runtime_ui_is_open(model)) frontend_screen(frontend.settings_return);
    } else if (frontend.screen == Screen::Characters) close_character_choice();
    else if (frontend.screen == Screen::Pause) frontend_resume();
}

void frontend_fonts() {
    if (!ui_texture_ready && ui_fonts_ready && ImGui::GetCurrentContext() && ImGui::GetIO().Fonts->TexID) {
        auto *texture = reinterpret_cast<SDL_Texture *>(static_cast<intptr_t>(ImGui::GetIO().Fonts->TexID));
        ui_texture_ready = SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest) == 0;
    }
    if (ui_fonts_ready || !ImGui::GetCurrentContext() || rom.size() != 393216) return;
    ui_fonts_ready = true;
    ui_art = smb3::menu_art(rom);
    auto *atlas = ImGui::GetIO().Fonts;
    if (atlas->Fonts.empty()) atlas->AddFontDefault();
    static const ImWchar range[] = {32, 32, 0};
    ImFontConfig config;
    config.SizePixels = 10;
    config.OversampleH = config.OversampleV = 1;
    config.PixelSnapH = true;
    config.GlyphRanges = range;
    ui_regular = atlas->AddFontDefault(&config);
    std::array<int, 224> rectangles{};
    for (int code = 32; code < 256; ++code)
        rectangles[code - 32] = atlas->AddCustomRectFontGlyph(ui_regular, code, 8, 10, 8);
    unsigned char *pixels = nullptr;
    int width, height;
    atlas->GetTexDataAsRGBA32(&pixels, &width, &height);
    // The ROM supplies the glyphs; nearest sampling preserves their square pixels at any UI scale.
    for (int code = 32; code < 256; ++code) {
        const auto glyph = smb3::menu_glyph(rom, code);
        const auto *rect = atlas->GetCustomRectByIndex(rectangles[code - 32]);
        for (int y = 0; y < 10; ++y)
            for (int x = 0; x < 8; ++x) {
                auto *pixel = pixels + ((rect->Y + y) * width + rect->X + x) * 4;
                pixel[0] = pixel[1] = pixel[2] = 255;
                pixel[3] = glyph[y * 8 + x] ? 255 : 0;
            }
    }
}

int frontend_event(const SDL_Event *event) {
    if (!frontend.active || cyc_state_ui_is_open()) return 0;
    using smb3::Screen;
    const bool key = event->type == SDL_KEYDOWN && !event->key.repeat;
    const bool pad = event->type == SDL_CONTROLLERBUTTONDOWN;
    if (event->type == SDL_CONTROLLERBUTTONUP && event->cbutton.button < SDL_CONTROLLER_BUTTON_MAX)
        consumed_controller_buttons &= ~(1u << event->cbutton.button);
    if (event->type == SDL_CONTROLLERDEVICEREMOVED ||
        (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_FOCUS_LOST))
        consumed_controller_buttons = 0;
    if (key || pad) { controller_hints = pad; ui_mouse = false; }
    if (event->type == SDL_CONTROLLERAXISMOTION && std::abs(event->caxis.value) > 24000) { controller_hints = true; ui_mouse = false; }
    if (event->type == SDL_MOUSEBUTTONDOWN || (event->type == SDL_MOUSEMOTION && (event->motion.xrel || event->motion.yrel))) {
        controller_hints = false;
        ui_mouse = true;
    }
    if (binding_button >= 0) {
        if (pad && event->cbutton.button < SDL_CONTROLLER_BUTTON_MAX)
            consumed_controller_buttons |= 1u << event->cbutton.button;
        if ((key && event->key.keysym.scancode == SDL_SCANCODE_ESCAPE) ||
            (pad && !binding_pad && event->cbutton.button == SDL_CONTROLLER_BUTTON_B)) {
            binding_button = -1;
            binding_status.clear();
            return 1;
        }
        int value = -1;
        if (key && event->key.keysym.scancode == SDL_SCANCODE_DELETE) value = 0;
        else if (key && !binding_pad) value = event->key.keysym.scancode;
        else if (pad && binding_pad) value = 1 + event->cbutton.button;
        else if (event->type == SDL_CONTROLLERAXISMOTION && binding_pad && std::abs(event->caxis.value) > 24000)
            value = 100 + event->caxis.axis * 2 + (event->caxis.value > 0);
        if (value >= 0) {
            if (smb3_ui_bind(binding_player, binding_button, binding_pad, value) >= 0) {
                binding_button = -1;
                binding_status = smb3_text("Binding saved.", "Control guardado.");
            } else binding_status = smb3_text("That input is a system shortcut. Choose another.",
                                              "Ese control es un atajo del sistema. Elige otro.");
        }
        return key || pad || event->type == SDL_CONTROLLERAXISMOTION;
    }
    const bool start = (key && !(event->key.keysym.mod & KMOD_ALT) &&
                        (event->key.keysym.scancode == SDL_SCANCODE_RETURN || event->key.keysym.scancode == SDL_SCANCODE_KP_ENTER)) ||
                       (pad && event->cbutton.button == SDL_CONTROLLER_BUTTON_START);
    if (frontend.screen == Screen::Intro && (start || (pad && event->cbutton.button == SDL_CONTROLLER_BUTTON_A))) {
        frontend.skip_intro = true;
        if (pad) consumed_controller_buttons |= 1u << event->cbutton.button;
        return 1;
    }
    if (start && frontend.screen == Screen::Playing && frontend.start_tick < 0 && !in_title(cyc_cpu_ram())) {
        if (pad) consumed_controller_buttons |= 1u << SDL_CONTROLLER_BUTTON_START;
        frontend_pause();
        return 1;
    }
    if (start && frontend.screen == Screen::Pause && pad) {
        for (auto &buttons : paused_buttons) buttons |= 0x10;
        frontend_resume();
        return 1;
    }
    if ((event->type == SDL_KEYDOWN || event->type == SDL_KEYUP) && event->key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
        if (key) {
            if (frontend.screen == Screen::Playing) frontend_pause();
            else frontend_back();
        }
        return 1;
    }
    return 0;
}

ImU32 ui_color(unsigned rgb) { return IM_COL32((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, 255); }
constexpr unsigned MenuInk = 0x292823, MenuPaper = 0xffedca, MenuGold = 0xf5d287;
constexpr unsigned MenuTrim = 0xd49648, MenuWhite = 0xfff9eb;

void ui_rect(ImDrawList *draw, float x, float y, float width, float height, unsigned color) {
    // Adjacent rows share the same edge even when a fractional UI scale rounds in different directions.
    auto edge = [](float value) { return std::floor(value + .001f); };
    draw->AddRectFilled(ImVec2(edge(x), edge(y)), ImVec2(edge(x + width), edge(y + height)), ui_color(color));
}

// Over the paused game the screens are dimmed, so their text switches from ink to white.
unsigned ui_ink() { return frontend.settings_return == smb3::Screen::Pause ? MenuWhite : MenuInk; }
void ui_dim(unsigned alpha) {
    ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(0, 0), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, alpha));
}

float ui_scale(ImVec2 screen) {
    return std::clamp(std::min(screen.x / 1000.f, screen.y / 720.f), .25f, 3.f);
}

void ui_text_at(const char *text, float size, float x, float y, unsigned rgb, bool outline) {
    ImFont *font = ui_regular ? ui_regular : ImGui::GetFont();
    auto *draw = ImGui::GetForegroundDrawList();
    const ImVec2 position(std::floor(x), std::floor(y));
    if (outline) {
        const float pixel = std::max(1.f, std::round(size / 10));
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if (dx || dy) draw->AddText(font, size, ImVec2(position.x + dx * pixel, position.y + dy * pixel), ui_color(0x171312), text);
    }
    draw->AddText(font, size, position, ui_color(rgb), text);
}
float ui_text_width(const char *text, float size) {
    ImFont *font = ui_regular ? ui_regular : ImGui::GetFont();
    return font->CalcTextSizeA(size, 100000, 0, text).x;
}
void ui_text(const char *text, float size, float y, unsigned rgb = 0x292823, bool outline = false) {
    ui_text_at(text, size, (ImGui::GetIO().DisplaySize.x - ui_text_width(text, size)) / 2, y, rgb, outline);
}
void ui_title(const char *text, float y, float scale) { ui_text(text, 30 * scale, y, MenuWhite, true); }
void ui_image(const Image &image, ImVec2 position, float pixel, SDL_Rect region) {
    auto *draw = ImGui::GetForegroundDrawList();
    for (int y = 0; y < region.h; ++y)
        for (int x = 0; x < region.w;) {
            const uint32_t color = image.at(region.x + x, region.y + y);
            const int start = x++;
            while (x < region.w && image.at(region.x + x, region.y + y) == color) ++x;
            if (color >> 24) ui_rect(draw, position.x + start * pixel, position.y + y * pixel,
                                    (x - start) * pixel, pixel, color);
        }
}
void ui_image(const Image &image, ImVec2 position, float pixel) {
    ui_image(image, position, pixel, {0, 0, image.width, image.height});
}
void ui_cursor(ImVec2 position, float scale) { ui_image(ui_art.cursor, position, 2 * scale); }
void ui_frame(ImVec2 top, ImVec2 bottom, float scale, ImDrawList *draw = nullptr) {
    if (!draw) draw = ImGui::GetForegroundDrawList();
    const float width = bottom.x - top.x, height = bottom.y - top.y;
    ui_rect(draw, top.x, top.y, width, height, MenuInk);
    ui_rect(draw, top.x + 3 * scale, top.y + 3 * scale, width - 6 * scale, height - 6 * scale, MenuGold);
    ui_rect(draw, top.x + 6 * scale, top.y + 6 * scale, width - 12 * scale, height - 12 * scale, MenuTrim);
    ui_rect(draw, top.x + 10 * scale, top.y + 10 * scale, width - 20 * scale, height - 20 * scale, MenuInk);
    for (float x : {top.x + 5 * scale, bottom.x - 9 * scale})
        for (float y : {top.y + 5 * scale, bottom.y - 9 * scale})
            ui_rect(draw, x, y, 4 * scale, 4 * scale, MenuWhite);
}
void ui_theater(float scale) {
    const ImVec2 screen = ImGui::GetIO().DisplaySize;
    auto *draw = ImGui::GetForegroundDrawList();
    const float pixel = 2 * scale, floor = screen.y - 72 * scale;
    const float band = 24 * scale;
    for (float y = 0; y < floor; y += band) {
        for (float x = 0; x < screen.x; x += band) {
            const float right = std::min(x + band, screen.x), bottom = std::min(y + band, floor);
            const auto color = smb3::stage_backdrop(0xff000000 | MenuPaper, (x + right) / 2,
                                                   (y + bottom) / 2, screen.x, floor);
            ui_rect(draw, x, y, right - x, bottom - y, color);
        }
    }
    if (!ui_art.curtain.empty()) {
        const float repeat = ui_art.curtain.width * pixel;
        for (float x = std::fmod(screen.x / 2, repeat) - repeat; x < screen.x; x += repeat)
            ui_image(ui_art.curtain, ImVec2(x, 0), pixel);
    }
    // The title's green hills keep the sides of a wide display part of the same stage.
    Image hill{80, 96, std::vector<uint32_t>(80 * 96)};
    for (int side = 0; side < 2; ++side) {
        for (int y = 0; y < hill.height; ++y)
            for (int x = 0; x < hill.width; ++x) hill.pixels[y * hill.width + x] = ui_art.scenery.at(x + side * 176, y + 96);
        ui_image(hill, ImVec2(side ? screen.x - 80 * pixel : 0, floor - 96 * pixel), pixel);
    }
    const float square = 16 * scale;
    ui_rect(draw, 0, floor, screen.x, screen.y - floor, smb3::StageInk);
    for (int y = 0; y < 2; ++y)
        for (float x = 0; x < screen.x; x += square * 2)
            ui_rect(draw, x + (y % 2) * square, floor + y * square, square, square, smb3::StageIvory);
}
void ui_logo(float y, float pixel) {
    ui_image(ui_art.title, ImVec2((ImGui::GetIO().DisplaySize.x - ui_art.title.width * pixel) / 2, y), pixel);
}
bool ui_option(const char *id, const char *label, ImVec2 position, ImVec2 size, float scale,
               bool selected = false, unsigned rgb = 0x292823) {
    ImGui::SetCursorScreenPos(position);
    const bool clicked = ImGui::InvisibleButton(id, size);
    const bool hover = ImGui::IsItemHovered();
    if (ui_mouse ? hover : selected) ui_cursor(ImVec2(position.x - 28 * scale, position.y + (size.y - 32 * scale) / 2), scale);
    ImGui::GetForegroundDrawList()->AddText(ImVec2(position.x, position.y + (size.y - ImGui::GetFontSize()) / 2), ui_color(rgb), label);
    return clicked;
}
bool ui_button(const char *id, const char *label, float y, float width, float scale, bool selected = false,
               unsigned rgb = 0x292823) {
    return ui_option(id, label, ImVec2((ImGui::GetIO().DisplaySize.x - width) / 2, y),
                     ImVec2(width, 40 * scale), scale, selected, rgb);
}
bool ui_pressed(ImGuiKey keyboard, ImGuiKey controller) {
    if (ImGui::IsKeyPressed(keyboard, false) ||
        (keyboard == ImGuiKey_Enter && ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) return true;
    if (consumed_controller_buttons) return false;
    if (ImGui::IsKeyPressed(controller, false)) return true;
    switch (controller) {
    case ImGuiKey_GamepadDpadUp: return ImGui::IsKeyPressed(ImGuiKey_GamepadLStickUp, false);
    case ImGuiKey_GamepadDpadDown: return ImGui::IsKeyPressed(ImGuiKey_GamepadLStickDown, false);
    case ImGuiKey_GamepadDpadLeft: return ImGui::IsKeyPressed(ImGuiKey_GamepadLStickLeft, false);
    case ImGuiKey_GamepadDpadRight: return ImGui::IsKeyPressed(ImGuiKey_GamepadLStickRight, false);
    default: return false;
    }
}
bool ui_confirm(bool start_allowed = false) {
    return ui_pressed(ImGuiKey_Enter, ImGuiKey_GamepadFaceDown) ||
        (start_allowed && !consumed_controller_buttons &&
         ImGui::IsKeyPressed(ImGuiKey_GamepadStart, false));
}
const char *ui_label(const char *label) {
    if (!smb3_ui_spanish()) return label;
    static const char *const labels[][2] = {
        {"Display", "Pantalla"}, {"Graphics", "Gráficos"}, {"Controls", "Controles"},
        {"System", "Sistema"}, {"Language", "Idioma"}, {"Practice", "Pruebas"},
        {"Fullscreen", "Pantalla completa"}, {"Window scale", "Tamaño de ventana"},
        {"Windowed", "Ventana"}, {"Borderless", "Sin bordes"}, {"Exclusive", "Exclusiva"},
        {"View mode", "Formato de pantalla"}, {"Native", "Nativo"}, {"16:9 fixed", "16:9 fijo"},
        {"Adaptive", "Adaptable"}, {"Integer scaling", "Escala entera"}, {"Linear filter", "Suavizado"},
        {"VSync", "Sincronización vertical"}, {"Volume", "Volumen"}, {"Audio synthesis", "Síntesis de sonido"},
        {"Legacy (default)", "Clásica"}, {"Band-limited", "Banda limitada"},
        {"Resume game", "Continuar"}, {"Quit", "Salir"}, {"Save states", "Partidas guardadas"},
        {"Rewind", "Rebobinar"}, {"Unavailable", "No disponible"}, {"On", "Sí"}, {"Off", "No"},
        {"Main menu", "Menú principal"}, {"Characters", "Personajes"}, {"Visual style", "Estilo visual"},
        {"Classic NES", "Clásico NES"}, {"Shadows", "Sombras"}, {"Tools", "Herramientas"},
        {"Background particles", "Partículas de fondo"}, {"Parallax", "Paralaje"},
        {"Star effect", "Efecto de estrella"}, {"Classic", "Clásico"}, {"Rainbow", "Arcoíris"},
        {"Up", "Arriba"}, {"Down", "Abajo"}, {"Left", "Izquierda"}, {"Right", "Derecha"},
        {"Space", "Espacio"}, {"Backspace", "Retroceso"}, {"Delete", "Suprimir"},
        {"dpup", "Cruceta arriba"}, {"dpdown", "Cruceta abajo"},
        {"dpleft", "Cruceta izq."}, {"dpright", "Cruceta der."},
        {"Press again to quit", "Pulsa otra vez para salir"}, {"Press again to confirm", "Pulsa otra vez para confirmar"},
        {"Done", "Listo"}, {"Saved", "Guardado"},
        {"Settings", "Configuración"}, {"PLAY", "JUGAR"}, {"QUIT GAME", "SALIR"},
        {"Browse For", "Buscar"}, {"Browse...", "Examinar..."}, {"Select file", "Elegir archivo"},
        {"Cancel", "Cancelar"}, {"< Back", "< Volver"}, {"File", "Archivo"}, {"Region", "Región"},
        {"Launching...", "Iniciando..."}, {"Copy details", "Copiar detalles"},
    };
    for (const auto &pair : labels) if (!SDL_strcasecmp(label, pair[0])) return pair[1];
    return label;
}
// Rows the title and pause menus already offer stay out of Settings.
bool ui_hidden_row(const RecompRuntimeUiRowView &row) {
    if (!row.item) return false;
    if (!std::strcmp(row.label, "Resume game") || !std::strcmp(row.item->key, "cyc.quit")) return true;
    return !std::strcmp(row.item->key, "smb3.home") && frontend.settings_return != smb3::Screen::Pause;
}
const char *ui_description(const RecompRuntimeUiRowView &row) {
    static const char *const sections[][3] = {
        {"Display", "Window, fullscreen and screen format.", "Ventana, pantalla completa y formato."},
        {"Graphics", "Scaling, smoothing and vertical sync.", "Escalado, suavizado y sincronización vertical."},
        {"Audio", "Sound output, volume and synthesis.", "Salida de sonido, volumen y síntesis."},
        {"System", "Language, save states and rewind.", "Idioma, partidas guardadas y rebobinado."},
        {"Controls", "Keyboard and controller bindings for each player.", "Teclado y mando de cada jugador."},
        {"Remastered", "Characters, visual style, shadows, particles, HUD and star.",
         "Personajes, estilo visual, sombras, partículas, HUD y estrella."},
        {"Practice", "Test powers, lives and levels.", "Prueba poderes, vidas y niveles."}};
    if (!row.item) {
        for (const auto &section : sections)
            if (!std::strcmp(row.label, section[0])) return smb3_text(section[1], section[2]);
        return "";
    }
    static const char *const spanish[][2] = {
        {"Choose windowed or fullscreen output.", "Elige ventana o pantalla completa."},
        {"Resize the window in native-size steps.", "Cambia el tamaño de la ventana. Solo en modo ventana."},
        {"Native, fixed widescreen, or live window aspect.", "Nativo, panorámico fijo o el formato de la ventana."},
        {"Snap output to whole native-pixel multiples.", "Ajusta la imagen a múltiplos exactos de píxel."},
        {"Smooth the final game image.", "Suaviza la imagen final del juego."},
        {"Enable or mute game audio output.", "Activa o silencia el sonido del juego."},
        {"Set the in-game mixer volume.", "Ajusta el volumen del juego."},
        {"Applies immediately. Legacy keeps the current sound.", "Se aplica al instante. Clásica conserva el sonido actual."},
        {"Choose the language of the menus.", "Elige el idioma de los menús."},
        {"Return to the title screen without ending the session.", "Vuelve al título sin terminar la partida."},
        {"Open character selection.", "Abre la selección de personaje."},
        {"Classic NES art keeps the widescreen view and every option below.",
         "El arte clásico de NES conserva la pantalla panorámica y las opciones de abajo."},
        {"Snow, embers, bubbles and wind over the scenery.", "Nieve, brasas, burbujas y viento sobre el escenario."},
        {"Far scenery scrolls slower than the ground.", "El fondo lejano se desplaza más despacio que el suelo."},
        {"Split panels at the screen edges, or the NES status bar.",
         "Paneles en los bordes de la pantalla o la barra de estado de NES."},
        {"Colors of the player during star power.", "Colores del jugador con la estrella."},
        {"Show scenery and character shadows.", "Muestra las sombras del escenario y de los personajes."},
        {"Test powers, lives, and levels.", "Prueba poderes, vidas y niveles."},
        {"Browse twelve save slots with thumbnails (F7).", "Doce ranuras de guardado con miniaturas (F7)."},
        {"Choose an earlier moment and resume there (F8).", "Elige un momento anterior y sigue desde ahí (F8)."}};
    const char *text = row.item->description ? row.item->description : "";
    if (!std::strcmp(row.item->key, "cyc.vsync"))
        return smb3_text("Wait for the display before each picture to avoid tearing. It never slows the game.",
                         "Espera a la pantalla antes de cada imagen para evitar cortes. Nunca frena el juego.");
    if (smb3_ui_spanish())
        for (const auto &pair : spanish) if (!std::strcmp(text, pair[0])) return pair[1];
    return text;
}
// Centers up to two lines that fit `width`.
void ui_wrapped(const char *text, float size, float y, float width, unsigned rgb) {
    std::string lines[2], word;
    int line = 0;
    for (const char *c = text;; ++c) {
        if (*c && *c != ' ') { word += *c; continue; }
        std::string candidate = lines[line].empty() ? word : lines[line] + " " + word;
        if (!lines[line].empty() && ui_text_width(candidate.c_str(), size) > width && line == 0) {
            line = 1;
            candidate = word;
        }
        lines[line] = candidate;
        word.clear();
        if (!*c) break;
    }
    for (int i = 0; i <= line; ++i) ui_text(lines[i].c_str(), size, y + i * size * 1.4f, rgb);
}

void ui_settings(float scale) {
    RecompRuntimeUi *model = cyc_ui_runtime();
    if (!recomp_runtime_ui_is_open(model)) { frontend_screen(frontend.settings_return); return; }
    auto input = [&](RecompRuntimeUiInput action) { recomp_runtime_ui_handle_input(model, action, 1, 0); };
    auto accept = [&]() {
        RecompRuntimeUiRowView row{};
        if (!recomp_runtime_ui_view_row(model, recomp_runtime_ui_view_selected_row(model), &row)) return;
        if (!row.item && !std::strcmp(row.label, "Controls")) frontend_screen(smb3::Screen::Controls);
        else if (!row.item && !std::strcmp(row.label, "Practice")) frontend_screen(smb3::Screen::Practice);
        else input(RECOMP_RUNTIME_UI_INPUT_ACCEPT);
    };
    const bool moved = ui_pressed(ImGuiKey_UpArrow, ImGuiKey_GamepadDpadUp) || ui_pressed(ImGuiKey_DownArrow, ImGuiKey_GamepadDpadDown);
    if (ui_pressed(ImGuiKey_UpArrow, ImGuiKey_GamepadDpadUp)) input(RECOMP_RUNTIME_UI_INPUT_UP);
    if (ui_pressed(ImGuiKey_DownArrow, ImGuiKey_GamepadDpadDown)) input(RECOMP_RUNTIME_UI_INPUT_DOWN);
    if (ui_pressed(ImGuiKey_LeftArrow, ImGuiKey_GamepadDpadLeft)) input(RECOMP_RUNTIME_UI_INPUT_LEFT);
    if (ui_pressed(ImGuiKey_RightArrow, ImGuiKey_GamepadDpadRight)) input(RECOMP_RUNTIME_UI_INPUT_RIGHT);
    if (ui_pressed(ImGuiKey_Enter, ImGuiKey_GamepadFaceDown)) accept();
    if (ui_pressed(ImGuiKey_Backspace, ImGuiKey_GamepadFaceRight)) input(RECOMP_RUNTIME_UI_INPUT_BACK);
    if (frontend.screen != smb3::Screen::Settings || !recomp_runtime_ui_is_open(model)) return;
    // Step over hidden rows in the direction the cursor moved (down when a section opens on one).
    const bool upward = ui_pressed(ImGuiKey_UpArrow, ImGuiKey_GamepadDpadUp);
    auto hidden = [&](size_t index) {
        RecompRuntimeUiRowView row{};
        return recomp_runtime_ui_view_row(model, index, &row) && ui_hidden_row(row);
    };
    for (size_t step = 0; step < recomp_runtime_ui_view_row_count(model) &&
         hidden(recomp_runtime_ui_view_selected_row(model)); ++step)
        input(upward ? RECOMP_RUNTIME_UI_INPUT_UP : RECOMP_RUNTIME_UI_INPUT_DOWN);
    RecompRuntimeUiRowView first{};
    recomp_runtime_ui_view_row(model, 0, &first);
    ui_title(first.item ? ui_label(first.item->section) : smb3_text("SETTINGS", "CONFIGURACIÓN"), 76 * scale, scale);
    const float width = std::min(ImGui::GetIO().DisplaySize.x - 80 * scale, 980 * scale);
    const float left = (ImGui::GetIO().DisplaySize.x - width) / 2;
    ui_frame(ImVec2(left, 148 * scale), ImVec2(left + width, 580 * scale), scale);
    ImGui::SetCursorScreenPos(ImVec2(left + 24 * scale, 174 * scale));
    ImGui::BeginChild("settings options", ImVec2(width - 48 * scale, 380 * scale), false, ImGuiWindowFlags_NoScrollbar);
    ImGui::GetForegroundDrawList()->PushClipRect(ImVec2(left + 24 * scale, 174 * scale), ImVec2(left + width - 24 * scale, 554 * scale), true);
    const size_t count = recomp_runtime_ui_view_row_count(model);
    for (size_t i = 0; i < count; ++i) {
        RecompRuntimeUiRowView row{};
        if (!recomp_runtime_ui_view_row(model, i, &row) || ui_hidden_row(row)) continue;
        ImGui::PushID(static_cast<int>(i));
        const ImVec2 position = ImGui::GetCursorScreenPos();
        const bool selected = i == recomp_runtime_ui_view_selected_row(model);
        auto select = [&]() {
            for (size_t step = 0; step <= count && recomp_runtime_ui_view_selected_row(model) != i; ++step)
                input(recomp_runtime_ui_view_selected_row(model) < i ? RECOMP_RUNTIME_UI_INPUT_DOWN : RECOMP_RUNTIME_UI_INPUT_UP);
        };
        const unsigned color = row.enabled ? (selected ? MenuGold : MenuWhite) : 0x888273;
        if (ui_option("settings row", ui_label(row.label), ImVec2(position.x + 28 * scale, position.y),
            ImVec2(width * .52f, 40 * scale), scale, selected, color) && row.enabled) { select(); accept(); }
        else if (ui_mouse && ImGui::IsItemHovered() && !selected && row.enabled) select();
        if (selected && (moved || ui_focus)) ImGui::SetScrollHereY(.5f);
        if (row.item && row.item->type != RECOMP_RUNTIME_UI_ACTION) {
            if (ui_option("decrease", "<", ImVec2(left + width * .59f, position.y), ImVec2(32 * scale, 40 * scale), scale, false, color) && row.enabled) {
                select(); input(RECOMP_RUNTIME_UI_INPUT_LEFT);
            }
            ImGui::GetForegroundDrawList()->AddText(ImVec2(left + width * .64f, position.y + 10 * scale), ui_color(color), ui_label(row.value));
            if (ui_option("increase", ">", ImVec2(left + width - 48 * scale, position.y), ImVec2(24 * scale, 40 * scale), scale, false, color) && row.enabled) {
                select(); input(RECOMP_RUNTIME_UI_INPUT_RIGHT);
            }
        } else ImGui::GetForegroundDrawList()->AddText(ImVec2(left + width - 48 * scale, position.y + 10 * scale), ui_color(color), ">");
        ImGui::SetCursorScreenPos(ImVec2(position.x, position.y + 40 * scale));
        ImGui::PopID();
    }
    ImGui::GetForegroundDrawList()->PopClipRect();
    ImGui::EndChild();
    RecompRuntimeUiRowView selected_row{};
    if (recomp_runtime_ui_view_row(model, recomp_runtime_ui_view_selected_row(model), &selected_row))
        ui_wrapped(ui_description(selected_row), 15 * scale, 516 * scale, width - 80 * scale, 0xb6b3a4);
    ui_text(ui_label(smb3_ui_status()), 15 * scale, 584 * scale, ui_ink());
    const unsigned back_color = ui_ink();
    const float center = ImGui::GetIO().DisplaySize.x / 2;
    if (ui_option("settings back", smb3_text("Back", "Volver"), ImVec2(center - 48 * scale, 602 * scale),
                  ImVec2(96 * scale, 40 * scale), scale, false, back_color))
        input(RECOMP_RUNTIME_UI_INPUT_BACK);
}
void ui_controls(float scale) {
    ui_title(smb3_text("CONTROLS", "CONTROLES"), 52 * scale, scale);
    const float width = std::min(ImGui::GetIO().DisplaySize.x - 80 * scale, 980 * scale);
    const float left = (ImGui::GetIO().DisplaySize.x - width) / 2;
    const bool editing = binding_button >= 0;
    const bool enter = !editing && ui_confirm();
    if (!editing && ui_pressed(ImGuiKey_UpArrow, ImGuiKey_GamepadDpadUp)) controls_row = (controls_row + 10) % 11;
    if (!editing && ui_pressed(ImGuiKey_DownArrow, ImGuiKey_GamepadDpadDown)) controls_row = (controls_row + 1) % 11;
    const int direction = editing ? 0 : ui_pressed(ImGuiKey_LeftArrow, ImGuiKey_GamepadDpadLeft) ? -1 :
                          ui_pressed(ImGuiKey_RightArrow, ImGuiKey_GamepadDpadRight) ? 1 : 0;
    if (controls_row == 0 && (direction || enter)) controls_player = 1 - controls_player;
    if (controls_row >= 2 && controls_row <= 9 && direction) controls_column = 1 - controls_column;
    if (!editing && controls_row >= 2 && controls_row <= 9 && ui_pressed(ImGuiKey_Delete, ImGuiKey_GamepadFaceUp) &&
        smb3_ui_bind(controls_player, controls_row - 2, controls_column, 0) >= 0)
        binding_status = smb3_text("Binding cleared.", "Asignación borrada.");
    for (int player = 0; player < 2; ++player) {
        ImGui::PushID(player);
        const char *label = player ? smb3_text("Player 2", "Jugador 2") : smb3_text("Player 1", "Jugador 1");
        if (ui_option("player", label, ImVec2(left + 60 * scale + player * 250 * scale, 100 * scale),
                      ImVec2(180 * scale, 40 * scale), scale, controls_row == 0 && controls_player == player,
                      controls_player == player ? ui_ink() : 0x888273)) { controls_player = player; controls_row = 0; }
        ImGui::PopID();
    }
    int source = smb3_ui_source(controls_player, -1);
    const char *const sources[] = {smb3_text("Input: none", "Entrada: ninguna"),
        smb3_text("Keyboard + controller", "Teclado + mando"), smb3_text("Input: controller", "Entrada: mando")};
    if (source >= 0 && source < 3) {
        const bool clicked = ui_option("input source", sources[source], ImVec2(left + 560 * scale, 100 * scale),
            ImVec2(390 * scale, 40 * scale), scale, controls_row == 1, ui_ink());
        if (clicked || (controls_row == 1 && (direction || enter))) {
            smb3_ui_source(controls_player, (source + (direction < 0 ? 2 : 1)) % 3);
            controls_row = 1;
        }
    }
    ui_frame(ImVec2(left, 150 * scale), ImVec2(left + width, 526 * scale), scale);
    auto *draw = ImGui::GetForegroundDrawList();
    draw->AddText(ImVec2(left + 48 * scale, 170 * scale), ui_color(MenuGold), smb3_text("ACTION", "ACCIÓN"));
    draw->AddText(ImVec2(left + 360 * scale, 170 * scale), ui_color(MenuGold), smb3_text("KEYBOARD", "TECLADO"));
    draw->AddText(ImVec2(left + 650 * scale, 170 * scale), ui_color(MenuGold), smb3_text("CONTROLLER", "MANDO"));
    const char *const names[] = {smb3_text("Jump / A", "Saltar / A"), smb3_text("Run / B", "Correr / B"),
        smb3_text("Select", "Selección"), smb3_text("Start", "Inicio"),
        ui_label("Up"), ui_label("Down"), ui_label("Left"), ui_label("Right")};
    for (int button = 0; button < 8; ++button) {
        const float y = (204 + button * 37) * scale;
        ImGui::GetForegroundDrawList()->AddText(ImVec2(left + 48 * scale, y + 7 * scale), ui_color(0xfff9eb), names[button]);
        for (int pad = 0; pad < 2; ++pad) {
            ImGui::PushID(button * 2 + pad);
            char label[128]; smb3_ui_bind_label(controls_player, button, pad, label, sizeof label);
            const bool waiting = binding_button == button && binding_pad == pad && binding_player == controls_player;
            const bool selected = controls_row == button + 2 && controls_column == pad;
            if (ui_option("binding", waiting ? "..." : *label ? ui_label(label) : "---",
                ImVec2(left + (pad ? 650 : 360) * scale, y), ImVec2(270 * scale, 35 * scale), scale, selected, 0xfff9eb) || (selected && enter)) {
                controls_row = button + 2; controls_column = pad;
                binding_player = controls_player; binding_button = button; binding_pad = pad;
                binding_status = pad ? smb3_text("Press a button or move an axis on your controller.",
                                                 "Pulsa un botón o mueve un eje del mando.")
                                     : smb3_text("Press the new key.", "Pulsa la nueva tecla.");
            } else if (ui_mouse && ImGui::IsItemHovered() && !editing) { controls_row = button + 2; controls_column = pad; }
            ImGui::PopID();
        }
    }
    ui_text(binding_status.empty() ? smb3_text("Choose an input to change its binding.", "Elige un control para cambiarlo.")
                                   : binding_status.c_str(), 15 * scale, 540 * scale, ui_ink());
    if (ui_button("controls back", smb3_text("Back", "Volver"), 572 * scale, 96 * scale, scale, controls_row == 10, ui_ink()) ||
        (controls_row == 10 && enter) || ui_pressed(ImGuiKey_Backspace, ImGuiKey_GamepadFaceRight)) frontend_back();
}

void ui_practice(float scale) {
    const char *const rows[] = {smb3_text("Power-up", "Poder"), smb3_text("Get 99 lives", "Obtener 99 vidas"),
        smb3_text("World", "Mundo"), smb3_text("Level", "Nivel"), smb3_text("Enter level", "Entrar al nivel"),
        smb3_text("Return to campaign", "Volver a campaña"), smb3_text("Back", "Volver")};
    constexpr int row_count = sizeof rows / sizeof rows[0];
    auto cycle = [](int value, int step, int count) { return (value + count + step) % count; };
    auto change = [&](int row, int step) {
        if (row == 0) practice_power = cycle(practice_power, step, sizeof PracticePowers / sizeof PracticePowers[0]);
        if (row == 2) {
            practice_world = cycle(practice_world, step, static_cast<int>(practice_levels.size()));
            practice_level = 0;
        }
        if (row == 3 && !practice_levels[practice_world].empty())
            practice_level = cycle(practice_level, step, static_cast<int>(practice_levels[practice_world].size()));
    };
    const bool available = smb3_ui_states_available();
    const bool in_level = practice_in_level();
    auto accept = [&](int row) {
        const auto &levels = practice_levels[practice_world];
        if (row == 6) frontend_back();
        if (!available) return;
        if (row == 0 && in_level) practice_apply_power(practice_power);
        if (row == 1 && (in_level || practice_on_map())) practice_give_lives();
        if (row == 4 && practice_boot.data && !levels.empty()) practice_warp(levels[practice_level], practice_power);
        if (row == 5 && practice_campaign.data) practice_restore_campaign();
    };
    if (ui_pressed(ImGuiKey_Backspace, ImGuiKey_GamepadFaceRight)) { frontend_back(); return; }
    if (ui_pressed(ImGuiKey_UpArrow, ImGuiKey_GamepadDpadUp)) practice_row = cycle(practice_row, -1, row_count);
    if (ui_pressed(ImGuiKey_DownArrow, ImGuiKey_GamepadDpadDown)) practice_row = cycle(practice_row, 1, row_count);
    if (ui_pressed(ImGuiKey_LeftArrow, ImGuiKey_GamepadDpadLeft)) change(practice_row, -1);
    if (ui_pressed(ImGuiKey_RightArrow, ImGuiKey_GamepadDpadRight)) change(practice_row, 1);
    if (ui_pressed(ImGuiKey_Enter, ImGuiKey_GamepadFaceDown)) accept(practice_row);
    if (frontend.screen != smb3::Screen::Practice) return;

    const auto &levels = practice_levels[practice_world];
    const std::string world = std::to_string(practice_world + 1);
    const std::string level = levels.empty() ? ui_label("Unavailable") : levels[practice_level].label(smb3_ui_spanish());
    const auto &power = PracticePowers[practice_power];
    const char *const values[] = {smb3_text(power[0], power[1]), nullptr, world.c_str(), level.c_str()};
    const bool enabled[] = {available, available && (in_level || practice_on_map()), true, !levels.empty(),
        available && practice_boot.data && !levels.empty(), available && bool(practice_campaign.data), true};
    ui_title(smb3_text("PRACTICE", "PRUEBAS"), 100 * scale, scale);
    const float width = 836 * scale;
    const float left = (ImGui::GetIO().DisplaySize.x - width) / 2;
    ui_frame(ImVec2(left, 163 * scale), ImVec2(left + width, 550 * scale), scale);
    for (int row = 0; row < row_count; ++row) {
        const float y = (185 + 48 * row) * scale;
        const unsigned color = !enabled[row] ? 0x888273 : row == practice_row ? MenuGold : MenuWhite;
        ImGui::PushID(row);
        if (ui_option("practice row", rows[row], ImVec2(left + 58 * scale, y), ImVec2(width * .42f, 40 * scale),
                      scale, row == practice_row, color) && enabled[row]) {
            practice_row = row;
            accept(row);
        } else if (ui_mouse && ImGui::IsItemHovered()) practice_row = row;
        if (row < 4 && values[row]) {
            const float value_x = left + 402 * scale;
            if (ui_option("previous value", "<", ImVec2(value_x, y), ImVec2(24 * scale, 40 * scale), scale, false, color)) {
                practice_row = row;
                change(row, -1);
            }
            ImGui::GetForegroundDrawList()->AddText(ImVec2(value_x + 42 * scale, y + 10 * scale),
                                                   ui_color(color), values[row]);
            if (ui_option("next value", ">", ImVec2(left + width - 55 * scale, y), ImVec2(24 * scale, 40 * scale),
                          scale, false, color)) {
                practice_row = row;
                change(row, 1);
            }
        }
        ImGui::PopID();
        if (frontend.screen != smb3::Screen::Practice) return;
    }
    const char *hint = !available ? smb3_text("Available in local play.", "Disponible en partida local.") : in_level
        ? smb3_text("Apply the power-up now or when entering a level.", "El poder se aplica al elegirlo o al entrar al nivel.")
        : smb3_text("The power-up applies when you enter the level.", "El poder se aplica al entrar al nivel.");
    ui_text(*practice_error ? practice_error : hint, 17 * scale, 574 * scale, ui_ink());
}

enum class HintIcon { Vertical, Horizontal, Confirm, Back, Start, Clear, Menu, Count };
struct Hint { HintIcon icon; const char *label; };

void ui_hints(float y, float scale, std::initializer_list<Hint> hints) {
    const auto &icons = image("Controls/Hints.png");
    const bool has_icons = icons.width == 32 * static_cast<int>(HintIcon::Count) && icons.height == 32;
    const char *const dpad = smb3_text("D-PAD", "CRUCETA");
    const char *const keys[][static_cast<int>(HintIcon::Count)] = {
        {smb3_text("UP/DOWN", "ARR/ABA"), smb3_text("LEFT/RIGHT", "IZQ/DER"), "ENTER", "ESC", "ENTER",
         smb3_text("DEL", "SUPR"), "ESC"},
        {dpad, dpad, "A", "B", "START", "Y", "RB"},
    };
    auto *font = ui_regular ? ui_regular : ImGui::GetFont();
    const float size = 16 * scale, gap = 18 * scale;
    auto measure = [&](const char *text) { return font->CalcTextSizeA(size, 100000, 0, text).x; };
    auto input_width = [&](HintIcon icon) {
        return has_icons ? 48 * scale : measure(keys[controller_hints][static_cast<int>(icon)]) + 14 * scale;
    };
    float width = -gap;
    for (const auto &hint : hints)
        width += input_width(hint.icon) + (*hint.label ? 6 * scale + measure(hint.label) : 0) + gap;
    float x = (ImGui::GetIO().DisplaySize.x - width) / 2;
    auto *draw = ImGui::GetForegroundDrawList();
    for (const auto &hint : hints) {
        const int cell = static_cast<int>(hint.icon);
        const float cap_width = input_width(hint.icon);
        if (has_icons) ui_image(icons, ImVec2(x, y), 1.5f * scale, {32 * cell, controller_hints ? 16 : 0, 32, 16});
        else {
            const unsigned face = !controller_hints ? MenuInk : hint.icon == HintIcon::Confirm ? 0x29634c :
                hint.icon == HintIcon::Back ? 0x7b3d35 : hint.icon == HintIcon::Clear ? 0x765221 : MenuInk;
            ui_rect(draw, x + 2 * scale, y + 2 * scale, cap_width, 24 * scale, MenuInk);
            ui_rect(draw, x, y, cap_width, 24 * scale, MenuGold);
            ui_rect(draw, x + 2 * scale, y + 2 * scale, cap_width - 4 * scale, 20 * scale, face);
            draw->AddText(font, size, ImVec2(x + 7 * scale, y + 4 * scale), ui_color(MenuWhite), keys[controller_hints][cell]);
        }
        x += cap_width;
        if (*hint.label) {
            x += 6 * scale;
            draw->AddText(font, size, ImVec2(x, y + 4 * scale), ui_color(MenuWhite), hint.label);
            x += measure(hint.label);
        }
        x += gap;
    }
}

void ui_characters(float scale) {
    const ImVec2 screen = ImGui::GetIO().DisplaySize;
    const float center = screen.x / 2, offset = (screen.y - 720 * scale) / 2;
    auto point = [&](float x, float y) { return ImVec2(center + x * scale, offset + y * scale); };
    auto *draw = ImGui::GetForegroundDrawList();
    auto rect = [&](float x, float y, float width, float height, unsigned color) {
        const ImVec2 top = point(x, y);
        ui_rect(draw, top.x, top.y, width * scale, height * scale, color);
    };
    auto hit = [&](const char *id, float x, float y, float width, float height) {
        ImGui::SetCursorScreenPos(point(x, y));
        return ImGui::InvisibleButton(id, ImVec2(width * scale, height * scale));
    };
    auto turn = [&](int direction) {
        character_choice.index = (character_choice.index + direction + character_count) % character_count;
    };
    if (ui_pressed(ImGuiKey_LeftArrow, ImGuiKey_GamepadDpadLeft)) turn(-1);
    if (ui_pressed(ImGuiKey_RightArrow, ImGuiKey_GamepadDpadRight)) turn(1);
    const bool choose_players = character_choice.start_campaign && !character_choice.load_save &&
        (character_choice.new_game || !frontend.started);
    if (choose_players &&
        (ui_pressed(ImGuiKey_UpArrow, ImGuiKey_GamepadDpadUp) || ui_pressed(ImGuiKey_DownArrow, ImGuiKey_GamepadDpadDown)))
        character_choice.players = 3 - character_choice.players;

    ui_frame(point(-376, 92), point(376, 612), scale);
    ui_text(smb3_text("SELECT CHARACTER", "ELIGE PERSONAJE"), 30 * scale, point(0, 116).y, 0xffffff, true);
    rect(-341, 160, 682, 2, MenuTrim);

    bool hover_arrow[2]{};
    for (int side = 0; side < 2; ++side) {
        if (hit(side ? "next character" : "previous character", side ? 134 : -322, 222, 188, 172)) turn(side ? 1 : -1);
        hover_arrow[side] = ImGui::IsItemHovered();
    }
    for (int i = 0; i < character_count; ++i) {
        ImGui::PushID(i);
        if (hit("select character", i * 30 - character_count * 15 + 4, 468, 22, 32)) character_choice.index = i;
        ImGui::PopID();
    }

    const int index = character_choice.index;
    const unsigned light[] = {0x654133, 0x4a5438, 0x3e5264, 0x644357};
    const unsigned name[] = {0xffc096, 0xb4e987, 0xb6dcfc, 0xfad5ee};
    // The stage light widens in square steps, like the scenery around it.
    for (int y = 180; y < 393; y += 4) {
        const int half = 15 + (y - 180) / 2;
        rect(-half, y, 2 * half, std::min(4, 393 - y), light[index]);
    }
    rect(-18, 175, 36, 6, MenuTrim);
    rect(-12, 181, 24, 4, MenuGold);
    rect(-342, 393, 684, 17, 0x523b28);
    rect(-342, 393, 684, 3, MenuTrim);
    rect(-115, 393, 230, 3, MenuGold);
    for (int x = -327; x < 342; x += 20) rect(x, 400, 10, 3, 0x775437);

    preview((index + character_count - 1) % character_count, point(-255 - 52, 393 - 104), 3.25f * scale, 125);
    if (character_count > 2)
        preview((index + 1) % character_count, point(255 - 52, 393 - 104), 3.25f * scale, 125);
    preview(index, point(-80, 393 - 160), 5 * scale, 255, true);
    for (int side = 0; side < 2; ++side) {
        const unsigned color = hover_arrow[side] ? MenuWhite : MenuGold;
        const int center_x = side ? 173 : -173;
        for (int i = 0; i < 5; ++i) {
            const int half = 3 + i * 2;
            rect(center_x + (side ? 2 - i : i - 2) * 4, 304 - half, 4, 2 * half, color);
        }
    }
    ui_text(Characters[index], 32 * scale, point(0, 420).y, name[index], true);
    for (int i = 0; i < character_count; ++i) {
        const int x = i * 30 - character_count * 15 + 8;
        rect(x, 477, 14, 12, i == index ? name[index] : 0x655747);
        rect(x + 3, 480, 8, 6, i == index ? MenuWhite : MenuInk);
    }

    if (*character_error) ui_text(character_error, 15 * scale, point(0, 520).y, 0xff9a8c);
    else if (character_choice.load_save) {
        const std::string world = std::string(smb3_text("WORLD ", "MUNDO ")) + std::to_string(campaign_save_world + 1);
        ui_text(world.c_str(), 20 * scale, point(0, 518).y, MenuGold);
    } else if (choose_players) {
        for (int players = 1; players <= 2; ++players) {
            const bool solo = players == 1;
            const char *label = solo ? smb3_text("1 PLAYER", "1 JUGADOR") : smb3_text("2 PLAYERS", "2 JUGADORES");
            if (ui_option(solo ? "single player" : "two players", label, point(solo ? -186 : 54, 513),
                          ImVec2(190 * scale, 38 * scale), scale, character_choice.players == players, MenuWhite))
                character_choice.players = players;
        }
    }
    const char *confirm = !character_choice.start_campaign ? smb3_text("CHOOSE", "ELEGIR") :
        character_choice.new_game || !frontend.started && !character_choice.load_save ? smb3_text("PLAY", "JUGAR") :
        smb3_text("CONTINUE", "CONTINUAR");
    const float action_y = 554;
    const bool accept = ui_option("confirm character", confirm, point(-162, action_y),
        ImVec2(110 * scale, 40 * scale), scale, false, MenuWhite)
        || ui_confirm(true);
    const bool back = ui_option("cancel character", smb3_text("BACK", "VOLVER"), point(66, action_y),
        ImVec2(110 * scale, 40 * scale), scale, false, MenuWhite) || ui_pressed(ImGuiKey_Backspace, ImGuiKey_GamepadFaceRight);
    const Hint character_hint{HintIcon::Horizontal, smb3_text("CHARACTER", "PERSONAJE")};
    const Hint confirm_hint{HintIcon::Confirm, smb3_text("SELECT", "ELEGIR")};
    const Hint back_hint{HintIcon::Back, smb3_text("BACK", "VOLVER")};
    const float hints_y = screen.y - 36 * scale;
    if (choose_players)
        ui_hints(hints_y, scale, {character_hint, {HintIcon::Vertical, smb3_text("PLAYERS", "JUGADORES")},
                                  confirm_hint, back_hint});
    else
        ui_hints(hints_y, scale, {character_hint, confirm_hint, back_hint});
    if (back) close_character_choice();
    else if (accept) {
        character = index;
        smb3_ui_save();
        if (!character_choice.start_campaign) close_character_choice();
        else if (character_choice.load_save) {
            if (load_campaign_save()) frontend_screen(smb3::Screen::Playing);
            else character_error = smb3_text("This saved game does not match this version. Start a new game.",
                                             "Esta partida guardada no es de esta versión. Empieza una nueva.");
        } else if (character_choice.new_game && frontend.started && !restart_campaign()) {
            character_error = smb3_text("Could not start a new game.", "No se pudo empezar una partida nueva.");
        } else {
            frontend.players = character_choice.players;
            frontend.play();
        }
    }
}

int ui_menu(const char *const *labels, int count, int &choice, float y, float scale, float row_height = 38) {
    if (ui_pressed(ImGuiKey_UpArrow, ImGuiKey_GamepadDpadUp)) choice = (choice + count - 1) % count;
    if (ui_pressed(ImGuiKey_DownArrow, ImGuiKey_GamepadDpadDown)) choice = (choice + 1) % count;
    int action = ui_confirm(frontend.screen != smb3::Screen::Pause) ? choice : -1;
    const float center = ImGui::GetIO().DisplaySize.x / 2;
    auto *font = ui_regular ? ui_regular : ImGui::GetFont();
    for (int i = 0; i < count; ++i) {
        const float size = 25 * scale;
        const float left = center - font->CalcTextSizeA(size, 100000, 0, labels[i]).x / 2;
        ImGui::SetCursorScreenPos(ImVec2(center - 190 * scale, y));
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("menu option", ImVec2(380 * scale, (row_height - 2) * scale))) action = i;
        if (ui_mouse && ImGui::IsItemHovered()) choice = i;
        if (choice == i) ui_cursor(ImVec2(left - 42 * scale, y), scale);
        ImGui::PopID();
        ui_text(labels[i], size, y, 0xffffff, true);
        y += row_height * scale;
    }
    return action;
}

// One row under the title logo, so the options stay clear of the demo actors on the floor.
int ui_menu_row(const char *const *labels, int count, int &choice, float y, float scale) {
    if (ui_pressed(ImGuiKey_LeftArrow, ImGuiKey_GamepadDpadLeft) || ui_pressed(ImGuiKey_UpArrow, ImGuiKey_GamepadDpadUp))
        choice = (choice + count - 1) % count;
    if (ui_pressed(ImGuiKey_RightArrow, ImGuiKey_GamepadDpadRight) || ui_pressed(ImGuiKey_DownArrow, ImGuiKey_GamepadDpadDown))
        choice = (choice + 1) % count;
    int action = ui_confirm(true) ? choice : -1;
    const float size = (count > 3 ? 22 : 24) * scale, gap = (count > 3 ? 46 : 58) * scale;
    float width = -gap;
    for (int i = 0; i < count; ++i) width += ui_text_width(labels[i], size) + gap;
    float x = (ImGui::GetIO().DisplaySize.x - width) / 2;
    for (int i = 0; i < count; ++i) {
        const float label_width = ui_text_width(labels[i], size);
        ImGui::SetCursorScreenPos(ImVec2(x - 42 * scale, y - 4 * scale));
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("menu option", ImVec2(label_width + 42 * scale, size + 8 * scale))) action = i;
        if (ui_mouse && ImGui::IsItemHovered()) choice = i;
        ImGui::PopID();
        if (choice == i) ui_cursor(ImVec2(x - 42 * scale, y), scale);
        ui_text_at(labels[i], size, x, y, 0xffffff, true);
        x += label_width + gap;
    }
    return action;
}

bool ui_open(void *) {
    return frontend.screen != smb3::Screen::Playing;
}
void ui_render(void *) {
    using smb3::Screen;
    frontend.active = true;
    if (cyc_state_ui_is_open()) return;
    if (frontend.screen == Screen::Playing && recomp_runtime_ui_is_open(cyc_ui_runtime())) {
        recomp_runtime_ui_close(cyc_ui_runtime());
        frontend_pause();
    }
    if (!ui_open(nullptr)) return;
    const ImVec2 screen = ImGui::GetIO().DisplaySize;
    SDL_FRect picture = smb3_picture_bounds();
    if (picture.w <= 0 || picture.h <= 0) picture = {0, 0, screen.x, screen.y};
    const Screen drawing_screen = frontend.screen;
    const float scale = ui_scale(screen);
    if (ui_regular) ImGui::PushFont(ui_regular);
    ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(screen);
    ImGui::SetNextWindowFocus();
    ImGui::SetNextWindowBgAlpha(0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, ui_color(0xfff9eb));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ui_color(0xffedca));
    const auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavInputs;
    ImGui::Begin("SMB3 Remastered", nullptr, flags);
    ImGui::SetWindowFontScale((ui_regular ? 2.f : 1.5f) * scale);
    auto *draw = ImGui::GetForegroundDrawList();
    const bool title_stage = frontend.shows_title() && !title_picture.empty();
    const Hint move_hint{HintIcon::Vertical, smb3_text("MOVE", "MOVER")};
    const Hint select_hint{HintIcon::Confirm, smb3_text("SELECT", "ELEGIR")};
    const Hint adjust_hint{HintIcon::Horizontal, smb3_text("ADJUST", "AJUSTAR")};
    const Hint back_hint{HintIcon::Back, smb3_text("BACK", "VOLVER")};
    if (drawing_screen == Screen::Notice) {
        ui_rect(draw, 0, 0, screen.x, screen.y, 0x000000);
        ui_text(smb3_text("NOTICE", "AVISO"), 36 * scale, screen.y * .14f, 0xf77446);
        ui_text(smb3_text("SMB3 REMASTERED IS AN UNOFFICIAL FAN PROJECT.",
                          "SMB3 REMASTERED ES UN PROYECTO NO OFICIAL DE FANS."), 24 * scale, screen.y * .35f, 0xffffff);
        ui_text(smb3_text("THE ORIGINAL GAME AND CHARACTERS BELONG TO NINTENDO.",
                          "EL JUEGO ORIGINAL Y SUS PERSONAJES SON DE NINTENDO."), 21 * scale, screen.y * .43f, 0xffffff);
        ui_text(smb3_text("PLAY WITH YOUR OWN COMPATIBLE NES ROM.",
                          "JUEGA CON TU PROPIA ROM COMPATIBLE DE NES."), 21 * scale, screen.y * .49f, 0xffffff);
        const char *labels[] = {smb3_text("CONTINUE", "CONTINUAR")};
        if (ui_menu(labels, 1, main_choice, screen.y * .69f, scale) == 0) frontend_intro();
        ui_hints(screen.y * .77f, scale, {{HintIcon::Confirm, ""}});
    } else if (drawing_screen == Screen::Intro) {
        if (frontend.intro_tick >= 15)
            ui_hints(picture.y + picture.h - 30 * scale, scale, {{HintIcon::Start, smb3_text("SKIP INTRO", "SALTAR INTRO")}});
    } else if (drawing_screen == Screen::Main || drawing_screen == Screen::Characters) {
        if (!title_stage) {
            ui_theater(scale);
            ui_logo(68 * scale, 2 * scale);
            picture = {0, 0, screen.x, screen.y};
        }
        if (drawing_screen == Screen::Characters) ui_characters(scale);
        else {
            const bool resume = frontend.started || campaign_save_world >= 0;
            const char *play[] = {smb3_text("PLAY", "JUGAR"), smb3_text("SETTINGS", "CONFIGURACIÓN"),
                                  smb3_text("QUIT", "SALIR")};
            const char *saved[] = {smb3_text("CONTINUE", "CONTINUAR"), smb3_text("NEW GAME", "NUEVA PARTIDA"),
                                   smb3_text("SETTINGS", "CONFIGURACIÓN"), smb3_text("QUIT", "SALIR")};
            const float title_scale = std::min(scale, picture.h / 720.f);
            const int count = resume ? 4 : 3;
            main_choice = std::min(main_choice, count - 1);
            int action = ui_menu_row(resume ? saved : play, count, main_choice, picture.y + picture.h * .615f, title_scale);
            if (!resume && action >= 1) ++action; // Same actions with or without the New game entry.
            if (action == 0) frontend_characters(Screen::Main, true);
            if (action == 1) frontend_characters(Screen::Main, true, true);
            if (action == 2) frontend_settings(Screen::Main);
            if (action == 3) { SDL_Event quit{}; quit.type = SDL_QUIT; SDL_PushEvent(&quit); }
            ui_hints(picture.y + picture.h - 38 * title_scale, title_scale,
                     {{HintIcon::Horizontal, smb3_text("MOVE", "MOVER")}, select_hint});
        }
    } else if (drawing_screen == Screen::Pause) {
        const float x = screen.x / 2, y = screen.y / 2;
        ui_dim(110);
        ui_frame(ImVec2(x - 216 * scale, y - 119 * scale), ImVec2(x + 216 * scale, y + 120 * scale), scale);
        ui_text(smb3_text("PAUSED", "PAUSA"), 27 * scale, y - 99 * scale, MenuGold);
        const char *labels[] = {smb3_text("RESUME", "REANUDAR"), smb3_text("SETTINGS", "CONFIGURACIÓN"),
                                smb3_text("TITLE SCREEN", "MENÚ PRINCIPAL")};
        const int action = ui_menu(labels, 3, pause_choice, y - 55 * scale, scale);
        if (action == 0 || ui_pressed(ImGuiKey_Backspace, ImGuiKey_GamepadFaceRight)) frontend_resume();
        if (action == 1) frontend_settings(Screen::Pause);
        if (action == 2) { // The session stays suspended; Continue on the title resumes it.
            release_pause();
            frontend_screen(Screen::Main);
        }
        ui_hints(y + 79 * scale, scale * .85f, {move_hint, select_hint, {HintIcon::Back, smb3_text("RESUME", "REANUDAR")}});
    } else {
        if (frontend.settings_return != Screen::Pause) ui_theater(scale);
        else ui_dim(170);
        if (drawing_screen == Screen::Settings) ui_settings(scale);
        else if (drawing_screen == Screen::Controls) {
            ui_controls(scale);
            if (binding_button >= 0)
                ui_hints(screen.y - 36 * scale, scale, {{HintIcon::Menu, smb3_text("CANCEL", "CANCELAR")}});
            else ui_hints(screen.y - 36 * scale, scale,
                          {move_hint, {HintIcon::Confirm, smb3_text("CHANGE", "CAMBIAR")},
                           {HintIcon::Clear, smb3_text("CLEAR", "BORRAR")}, back_hint});
        } else if (drawing_screen == Screen::Practice) {
            ui_practice(scale);
            ui_hints(screen.y - 36 * scale, scale, {move_hint, adjust_hint, select_hint, back_hint});
        }
        if (drawing_screen == Screen::Settings)
            ui_hints(screen.y - 36 * scale, scale, {move_hint, adjust_hint, select_hint, back_hint});
    }
    if (frontend.screen == drawing_screen) ui_focus = false;
    ImGui::End(); ImGui::PopStyleColor(2); ImGui::PopStyleVar();
    if (ui_regular) ImGui::PopFont();
}
