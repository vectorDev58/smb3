#ifdef CYC_WITH_RECOMP_UI
#include "ui_host.h"
#include "ui_language.h"
#include "cyc_ui.h"
#include "cyc_host_extras.h"
#include "cyc_settings.h"
#include "cyc_input.h"
#include "launcher_files.h"
#include "recomp_launcher.h"
#include "rom_validation.h"
#include "pad_choice.h"
#include "recomp_runtime_ui.h"
#include "recomp_runtime_ui_internal.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CycUiHost menu_host;
extern RecompRuntimeUi *cyc_ui_runtime(void);
extern RecompRuntimeUi *smb3_create_runtime_menu(const CycUiHost *host);

static bool valid_rom_file(const char *path) {
    SDL_RWops *file = SDL_RWFromFile(path, "rb");
    if (!file) return false;
    uint8_t *data = SDL_RWsize(file) == SMB3_ROM_BYTES ? malloc(SMB3_ROM_BYTES) : NULL;
    const bool valid = data && SDL_RWread(file, data, 1, SMB3_ROM_BYTES) == SMB3_ROM_BYTES &&
        smb3_valid_rom(data, SMB3_ROM_BYTES);
    free(data);
    SDL_RWclose(file);
    return valid;
}

static bool cached_rom(const char *cache, char *path, size_t capacity) {
    if (!cache || capacity < 2) return false;
    SDL_RWops *file = SDL_RWFromFile(cache, "rb");
    if (!file) return false;
    const Sint64 size = SDL_RWsize(file);
    const bool read = size > 0 && (size_t)size < capacity && SDL_RWread(file, path, 1, (size_t)size) == (size_t)size;
    SDL_RWclose(file);
    if (!read) return false;
    path[size] = '\0';
    path[strcspn(path, "\r\n")] = '\0';
    return valid_rom_file(path);
}

int smb3_launcher_run_window(const char *title, RecompLauncherCSettings *settings,
                             const RecompLauncherCGameInfo *game, const char *assets,
                             const char *initial_rom, char *selected_rom, size_t capacity) {
    if (initial_rom && *initial_rom) return RECOMP_LAUNCHER_RESULT_LAUNCH;
    const char *cache = game->rom_cache_path;
    if (cached_rom(cache, selected_rom, capacity)) return RECOMP_LAUNCHER_RESULT_LAUNCH;
    const char *patterns[] = {"*.nes", "*.NES"};
    while (true) {
        const int picked = launcher_try_pick_file(
            smb3_text("SMB3 Remastered — Select your ROM (USA, PRG0)", "SMB3 Remastered — Elige tu ROM (USA, PRG0)"),
            patterns, 2, smb3_text("NES ROM", "ROM de NES"), selected_rom, capacity);
        if (picked == 0) return RECOMP_LAUNCHER_RESULT_QUIT;
        if (picked < 0) break;
        if (!valid_rom_file(selected_rom)) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, smb3_text("Unsupported ROM", "ROM incompatible"),
                smb3_text("Select Super Mario Bros. 3 (USA), revision PRG0.",
                          "Elige Super Mario Bros. 3 (USA), revisión PRG0."), NULL);
            continue;
        }
        if (cache && cyc_host_saves_enabled()) {
            SDL_RWops *file = SDL_RWFromFile(cache, "wb");
            if (file) {
                SDL_RWwrite(file, selected_rom, 1, strlen(selected_rom));
                SDL_RWclose(file);
            }
        }
        return RECOMP_LAUNCHER_RESULT_LAUNCH;
    }
    // Keep the dependency's in-app browser on systems without a native picker.
    RecompLauncherCGameInfo cartridge = *game;
    cartridge.has_bios = 0;
    cartridge.host_persists_paths = !cyc_host_saves_enabled();
    return recomp_launcher_run_window(smb3_text(title, "SMB3 Remastered - Selector de ROM"),
                                     settings, &cartridge, assets, initial_rom, selected_rom, capacity);
}

RecompRuntimeUi *cyc_ui_menu_create(const CycUiHost *host) {
    menu_host = *host;
    return smb3_create_runtime_menu(host);
}

// Pads in NESRecomp's order (cyc_sdl.c open_pads): SDL's game controllers by device index.
static int picked_pad[CYC_INPUT_PLAYERS] = {-1, -1}; // SDL instance ids chosen in Controls
static int pad_in_use[CYC_INPUT_PLAYERS] = {-1, -1}, coop_pad = -1;

static int pad_device(int pad) {
    for (int i = 0, n = 0; i < SDL_NumJoysticks() && n < CYC_INPUT_MAX_PADS; ++i)
        if (SDL_IsGameController(i) && n++ == pad) return i;
    return -1;
}

static bool frame_pad_ids(const CycInputFrame *f, int *ids) {
    char guid[40];
    for (int pad = 0; pad < f->pad_count; ++pad) {
        const int device = pad_device(pad);
        if (device < 0 || !f->pad_guid[pad]) return false;
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(device), guid, sizeof guid);
        if (strcmp(guid, f->pad_guid[pad])) return false;
        ids[pad] = SDL_JoystickGetDeviceInstanceID(device);
    }
    return pad_device(f->pad_count) < 0;
}

// cyc_sdl.c reads the controllers through here (CMakeLists.txt). NESRecomp's own evaluation keeps
// the shortcuts of every pad; a player whose pad src/pad_choice.h changes is evaluated alone, as
// player 1 with only that pad.
void smb3_input_eval(const CycBindings *b, const CycInputFrame *f, CycInputState *out) {
    cyc_input_eval(b, f, out);
    int ids[CYC_INPUT_MAX_PADS], pad[CYC_INPUT_PLAYERS];
    // Until NESRecomp reopens its pads after a change, its own choice stands.
    if (!frame_pad_ids(f, ids)) { memcpy(pad_in_use, out->player_pad, sizeof pad_in_use); return; }
    smb3_choose_pads(b, f, ids, picked_pad, smb3_coop_enabled(), &coop_pad, pad);
    memcpy(pad_in_use, pad, sizeof pad_in_use);
    if (!memcmp(pad, out->player_pad, sizeof pad)) return;
    for (int p = 0; p < CYC_INPUT_PLAYERS; ++p) {
        CycBindings alone = *b;
        alone.source[0] = b->source[p];
        alone.device[0][0] = '\0';
        alone.deadzone[0] = b->deadzone[p];
        memcpy(alone.button[0], b->button[p], sizeof alone.button[0]);
        memcpy(alone.game_button[0], b->game_button[p], sizeof alone.game_button[0]);
        for (int other = 1; other < CYC_INPUT_PLAYERS; ++other) alone.source[other] = 0;
        CycInputFrame own = *f;
        own.pad_count = pad[p] >= 0;
        if (pad[p] >= 0) {
            own.pad_buttons[0] = f->pad_buttons[pad[p]];
            memcpy(own.pad_axes[0], f->pad_axes[pad[p]], sizeof own.pad_axes[0]);
            own.pad_guid[0] = f->pad_guid[pad[p]];
        }
        CycInputState state;
        cyc_input_eval(&alone, &own, &state);
        out->buttons[p] = state.buttons[0];
        out->game_buttons[p] = state.game_buttons[0];
        out->player_pad[p] = pad[p];
    }
}

int smb3_ui_pads(void) {
    int pads = 0;
    while (pad_device(pads) >= 0) ++pads;
    return pads;
}
void smb3_ui_pad_name(int pad, char *out, size_t size) {
    if (!out || !size) return;
    const char *name = pad_device(pad) >= 0 ? SDL_GameControllerNameForIndex(pad_device(pad)) : NULL;
    snprintf(out, size, "%s", name ? name : "");
}
int smb3_ui_pad_in_use(int player) {
    return player >= 0 && player < CYC_INPUT_PLAYERS ? pad_in_use[player] : -1;
}
// The pad chosen for a player in Controls, -1 for automatic.
int smb3_ui_chosen_pad(int player) {
    if (!menu_host.settings || player < 0 || player >= CYC_INPUT_PLAYERS) return -1;
    const char *saved = menu_host.settings->bind.device[player];
    char guid[40];
    int model = -1; // As src/pad_choice.h: that exact pad, else the first of its model.
    for (int pad = 0, device; (device = pad_device(pad)) >= 0; ++pad) {
        if (SDL_JoystickGetDeviceInstanceID(device) == picked_pad[player]) return pad;
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(device), guid, sizeof guid);
        if (model < 0 && saved[0] && !strcmp(guid, saved)) model = pad;
    }
    return model;
}
// pad -1 returns the player to automatic. The other player gives up that pad.
void smb3_ui_choose_pad(int player, int pad) {
    if (!menu_host.settings || player < 0 || player >= CYC_INPUT_PLAYERS) return;
    CycBindings *bind = &menu_host.settings->bind;
    const int device = pad >= 0 ? pad_device(pad) : -1;
    const int id = device >= 0 ? SDL_JoystickGetDeviceInstanceID(device) : -1;
    char guid[40] = "";
    if (device >= 0) SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(device), guid, sizeof guid);
    for (int other = 0; other < CYC_INPUT_PLAYERS; ++other)
        if (other != player && device >= 0 && smb3_ui_chosen_pad(other) == pad) {
            picked_pad[other] = -1;
            bind->device[other][0] = '\0';
        }
    picked_pad[player] = id;
    snprintf(bind->device[player], sizeof bind->device[player], "%s", guid);
    smb3_ui_save();
    SDL_GameController *chosen = id >= 0 ? SDL_GameControllerFromInstanceID(id) : NULL;
    if (chosen) SDL_GameControllerRumble(chosen, 0x6000, 0x6000, 250); // Shows which of two alike pads it is.
}
void smb3_ui_save(void) { if (menu_host.save) menu_host.save(); }
bool smb3_ui_states_available(void) {
    return menu_host.state_restore && (!menu_host.states_available || menu_host.states_available());
}
bool smb3_ui_restore(const uint8_t *data, size_t size, char *error, size_t capacity) {
    return menu_host.state_restore && menu_host.state_restore(data, size, error, capacity);
}
const char *smb3_ui_status(void) {
    RecompRuntimeUi *model = cyc_ui_runtime();
    // The pinned model exposes rows publicly; its confirmation prompt still belongs to the renderer.
    if (!model || model->status_frames <= 0) return "";
    --model->status_frames;
    return model->status;
}
int smb3_ui_source(int player, int replacement) {
    if (!menu_host.settings || player < 0 || player >= CYC_INPUT_PLAYERS) return -1;
    if (replacement >= 0 && replacement <= 2) {
        CycBindings *bind = &menu_host.settings->bind;
        bool keys = false;
        for (int button = 0; button < CYC_INPUT_BUTTONS; ++button) keys |= bind->button[player][button].key != 0;
        // A player moved to the keyboard without keys of its own (player 2 has none by default)
        // takes the other player's keys; either can still remap them below.
        if (replacement == 1 && !keys)
            for (int button = 0; button < CYC_INPUT_BUTTONS; ++button)
                bind->button[player][button].key = bind->button[1 - player][button].key;
        bind->source[player] = replacement;
        smb3_ui_save();
    }
    return menu_host.settings->bind.source[player];
}
int smb3_ui_bind(int player, int button, int gamepad, int replacement) {
    if (!menu_host.settings || player < 0 || player >= CYC_INPUT_PLAYERS || button < 0 || button >= 8) return -1;
    CycBinding *binding = &menu_host.settings->bind.button[player][button];
    if (replacement >= 0) {
        const int valid = gamepad ? replacement == 0 ||
            (replacement >= 1 && replacement <= SDL_CONTROLLER_BUTTON_MAX) ||
            (replacement >= 100 && replacement < 100 + SDL_CONTROLLER_AXIS_MAX * 2) :
            replacement < SDL_NUM_SCANCODES;
        if (!valid) return -1;
        for (int shortcut = 0; replacement && shortcut < CYC_SC_COUNT; ++shortcut)
            if (replacement == (gamepad ? menu_host.settings->bind.shortcut[shortcut].pad :
                               menu_host.settings->bind.shortcut[shortcut].key)) return -1;
        if (gamepad) binding->pad = replacement;
        else binding->key = replacement;
        smb3_ui_save();
    }
    return gamepad ? binding->pad : binding->key;
}
void smb3_ui_bind_label(int player, int button, int gamepad, char *out, size_t size) {
    if (!out || !size) return;
    out[0] = 0;
    if (!menu_host.settings || player < 0 || player >= CYC_INPUT_PLAYERS || button < 0 || button >= 8) return;
    CycBinding binding = menu_host.settings->bind.button[player][button];
    if (gamepad ? !binding.pad : !binding.key) return;
    if (gamepad) binding.key = 0; else binding.pad = 0;
    cyc_binding_hint(&binding, out, size);
}
#endif
