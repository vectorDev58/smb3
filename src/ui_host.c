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
#include "recomp_runtime_ui.h"
#include "recomp_runtime_ui_internal.h"
#include <SDL.h>
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
        menu_host.settings->bind.source[player] = replacement;
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
