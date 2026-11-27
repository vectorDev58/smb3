#include "pixels.h"
#include "backdrops.h"
#include "characters.h"
#include "resource_packs.h"
#include "frontend_flow.h"
#include "menu_art.h"
#include "ui_language.h"
#include "cyc_core.h"
#include "cyc_host_extras.h"
#include "cyc_mod.h"
#include "cyc_render.h"
#include "cyc_run.h"
#include "cyc_video.h"
#include "actors.h"
#include "coop.h"
#include "mod_savestate.h"
#include "mod_function_hooks.h"
#include <cstdio>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include "webp/decode.h"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_MAX_DIMENSIONS 8192
#include "stb_image.h"

#ifdef CYC_WITH_SDL
#include <SDL.h>
#endif
#ifdef CYC_WITH_RECOMP_UI
#include "imgui.h"
#include "recomp_runtime_ui.h"
#include "cyc_state.h"
#include "cyc_ui.h"
#include "cyc_state_ui.h"
#include "practice.h"
#include "ui_host.h"
#include "window.h"
#include "state_menu_style.h"
#include <memory>
extern "C" RecompRuntimeUi *cyc_ui_runtime(void);
#endif

namespace {
using smb3::Image;
#ifdef CYC_WITH_RECOMP_UI
void frontend_screen(smb3::Screen screen);
void frontend_characters(smb3::Screen back, bool start_campaign, bool new_game = false);
void read_campaign_save_info();
void release_pause();
#endif
constexpr int NativeWidth = 256, Height = 240, PlayfieldHeight = 192;
constexpr int LevelRows = 27, LevelColumns = NativeWidth / 16;
constexpr int LevelScreenTiles = LevelRows * LevelColumns;
constexpr size_t ChrStart = 262144;
constexpr uint8_t ResultChrBank = 0x5e;
constexpr uint32_t GoalBlack = 0xff000000;
const char *const Characters[] = {"Mario", "Luigi", "Toad", "Toadette"};
// enhanced picks the art; the widescreen view and the other options apply to both styles.
int character = 0, partner = 1, enhanced = 1, shadows = 1, view = 2; // partner: player 2's character in co-op
int particles = 1, parallax = 1, modern_hud = 1, rainbow_star = 0;
bool spanish_ui = false;
int character_count = 2;
std::string requested_level; // --level: automated tests start in this practice level.
smb3::FrontendFlow frontend;
#ifdef CYC_WITH_RECOMP_UI
constexpr uint16_t VBlankTick = 0x10;
constexpr uint16_t CommonWait = 0x96fb, TitleWait = 0xa81c;
uint16_t paused_wait = 0;
Image paused_picture, title_picture;
int title_native_origin = 0;
std::array<uint8_t, 2> paused_buttons{};
#endif
bool art_loaded = false;
std::filesystem::path game_directory; // Beside the executable: resource_packs/, saves/.
std::vector<std::filesystem::path> pack_sprites; // resource_packs/*/Sprites, as in SMB1 Remastered packs.
std::unordered_map<std::string, Image> images;
std::unordered_map<uint32_t, unsigned> frame_colors;
std::vector<uint8_t> rom;
std::array<uint32_t, NativeWidth * Height> background;
std::vector<uint32_t> sprite_layer;
std::array<uint8_t, 32 * 32> original_player;
std::array<uint8_t, 256> frame_oam;
std::array<uint8_t, 2048> frame_ram;
std::array<uint8_t, 32> frame_palette;
std::array<uint8_t, 8192> frame_cart;
size_t frame_cart_size = 0;
bool snapshot_valid = false;
void (*following_observer)(void) = nullptr;
int following_line = -1;
struct ScreenEffect { int x8 = 0, y = -1, spread = 0, tile = -1, real = 0, seen = -2; };
std::array<ScreenEffect, 15> screen_effects; // Kill flash, 5 scores, 4 coins, 2 brick busts, 3 splashes.
int effect_tick = 0; // Emulated frames, for the effects seen in this frame or the one before.

#ifdef CYC_WITH_RECOMP_UI
constexpr int PracticeWing = 7, PracticeStar = 8;
const char *const PracticePowers[][2] = {
    {"Small", "Pequeño"}, {"Super", "Súper"}, {"Fire", "Fuego"}, {"Raccoon", "Mapache"},
    {"Frog", "Rana"}, {"Tanooki", "Tanooki"}, {"Hammer", "Martillo"}, {"P-Wing", "Ala P"}, {"Star", "Estrella"},
};
struct PracticeState {
    std::unique_ptr<uint8_t, decltype(&std::free)> data{nullptr, std::free};
    size_t size = 0;

    bool capture() {
        uint8_t *blob = nullptr;
        char error[256]{};
        if (!cyc_state_save(&blob, &size, error, sizeof error)) {
            std::fprintf(stderr, "Pruebas: %s\n", error);
            return false;
        }
        data.reset(blob);
        return true;
    }

    bool restore() const {
        char error[256]{};
        if (data && smb3_ui_restore(data.get(), size, error, sizeof error)) return true;
        std::fprintf(stderr, "Pruebas: %s\n", *error ? error : "Estado no disponible");
        return false;
    }
};
smb3::PracticeLevels practice_levels;
PracticeState practice_boot, practice_campaign;
smb3::FrontendFlow campaign_frontend;
const smb3::PracticeLevel *practice_target = nullptr;
int practice_warp_power = 0, practice_warp_suit = 0, practice_warp_lives = -1;
const char *practice_error = "";

int practice_start_world(uint16_t) {
    if (practice_target) cyc_mod_poke(0x727, practice_target->world); // World_Num
    return 0;
}

int practice_enter_level(uint16_t) {
    const auto *ram = cyc_cpu_ram();
    if (!practice_target || ram[0x729] != 0x0d || ram[0x727] != practice_target->world) return 0;
    const auto &level = *practice_target;
    const int player = ram[0x726] & 1;
    const int page = level.screen_column >> 4;
    cyc_mod_poke(0x75 + player, level.y);                        // World_Map_Y
    cyc_mod_poke(0x77 + player, page);                           // World_Map_XHi
    cyc_mod_poke(0x79 + player, (level.screen_column & 15) * 16); // World_Map_X
    cyc_mod_poke(0x12, page);                                   // Horz_Scroll_Hi for map return
    cyc_mod_poke(0xfd, 0);                                      // Horz_Scroll
    cyc_mod_poke(0x598, level.world == 7 && page == 2);           // World_8_Dark
    cyc_mod_poke(0x1e, level.entry);                            // Map_EnterViaID
    cyc_mod_poke(0x746 + player, practice_warp_suit);            // World_Map_Power
    cyc_mod_poke(0x3f3, practice_warp_power == PracticeWing ? 8 : practice_warp_suit);
    cyc_mod_poke(0x3f2, practice_warp_power == PracticeStar);    // Map_Starman
    if (practice_warp_lives >= 0) cyc_mod_poke(0x736 + player, practice_warp_lives);
    // Run the normal map exit after its operation dispatch has already finished for this frame.
    cyc_mod_poke(0x729, 0x10);
    practice_target = nullptr;
    return 0;
}
#endif

const uint8_t *oam() { return snapshot_valid ? frame_oam.data() : cyc_render_oam(); }

void snapshot() {
    // SMB3's status split can alter OAM/CHR later in the frame. Keep the playfield's data.
    std::copy_n(cyc_render_oam(), frame_oam.size(), frame_oam.begin());
    std::copy_n(cyc_cpu_ram(), frame_ram.size(), frame_ram.begin());
    for (int i = 0; i < 32; ++i) frame_palette[i] = cyc_render_palette(i);
    size_t size = 0;
    const uint8_t *cart = cyc_cart_ram(&size);
    frame_cart_size = std::min(size, frame_cart.size());
    if (cart && frame_cart_size) std::copy_n(cart, frame_cart_size, frame_cart.begin());
    snapshot_valid = true;
    smb3::displayed_extra_sprites = smb3::extra_sprites;
    hw_observe_line = following_line > 1 ? following_line : -1;
    cyc_run_observer = following_line > 1 ? following_observer : nullptr;
}

void frame_begin(void *) {
    following_observer = cyc_run_observer;
    following_line = following_observer ? hw_observe_line : -1;
    hw_observe_line = 1;
    cyc_run_observer = snapshot;
}

int actor_visibility(uint16_t) { return smb3::wide_visibility(cyc_cpu_ram(), true); }
int actor_spawning(uint16_t) {
    smb3::spawn_visible_actors(cyc_cpu_ram(), true);
    return 0;
}
int projectile_origin(uint16_t) { return smb3::projectile_origin(cyc_cpu_ram(), true); }
int update_projectile(uint16_t) {
    smb3::coop_special_begin(cyc_cpu_ram()); // Undone after the object's turn by smb3.coop-special-done.
    return smb3::update_projectile(cyc_cpu_ram(), true);
}
int projectile_hit(uint16_t) { return smb3::projectile_hit(cyc_cpu_ram()); }
int update_player_projectile(uint16_t) { return smb3::update_projectile(cyc_cpu_ram(), true, true); }
int player_projectile_hit(uint16_t) { return smb3::player_projectile_hit(cyc_cpu_ram()); }
int update_cannon(uint16_t) { return smb3::update_cannon(cyc_cpu_ram(), true); }
int actor_draw_start(uint16_t) {
    smb3::coop_object_begin(cyc_cpu_ram());
    return smb3::actor_drawing(cyc_cpu_ram(), true, true);
}
int actor_draw_end(uint16_t) {
    const int handled = smb3::actor_drawing(cyc_cpu_ram(), true, false);
    smb3::coop_object_end(cyc_cpu_ram());
    return handled;
}
int coop_player_hook(uint16_t) { return smb3::coop_player_update(cyc_cpu_ram()); }
int coop_player_done_hook(uint16_t) { return smb3::coop_player_done(cyc_cpu_ram()); }
int coop_scrolling_hook(uint16_t) { return smb3::coop_scrolling(cyc_cpu_ram()); }
int coop_die_hook(uint16_t) { return smb3::coop_die(cyc_cpu_ram()); }
int coop_throw_hook(uint16_t) { return smb3::coop_throw(cyc_cpu_ram()); }
int coop_special_done_hook(uint16_t) { smb3::coop_object_end(cyc_cpu_ram()); return 0; }
int coop_skip_mushroom_hook(uint16_t site) { return smb3::coop_skip_mushroom(cyc_cpu_ram(), site); }
int actor_keep_visible(uint16_t) { return smb3::keep_visible_actor(cyc_cpu_ram(), true); }
int remember_collision_box(uint16_t) { return smb3::remember_collision_box(cyc_cpu_ram(), true); }
int intersect_world_boxes(uint16_t) { return smb3::intersect_world_boxes(cyc_cpu_ram(), true); }
int shell_block_boundary(uint16_t) { return smb3::shell_block_boundary(cyc_cpu_ram(), true); }
int player_crush_boundary(uint16_t) { return smb3::player_crush_boundary(cyc_cpu_ram(), true); }
int player_window_boundary(uint16_t) { return smb3::player_window_boundary(cyc_cpu_ram(), true); }
int autoscroll_player(uint16_t) { return smb3::autoscroll_player(cyc_cpu_ram(), true); }

const int ActorHooksRegistered[] = {
    nes_mod_register_function_entry_plugin("smb3.wide-visibility", 0xD7AB, actor_visibility),
    nes_mod_register_function_entry_plugin("smb3.wide-spawn", 0xB836, actor_spawning),
    nes_mod_register_function_entry_plugin("smb3.wide-projectile-origin", 0xC454, projectile_origin),
    nes_mod_register_function_entry_plugin("smb3.wide-projectile", 0xAF24, update_projectile),
    nes_mod_register_function_entry_plugin("smb3.wide-projectile-hit", 0xB7D1, projectile_hit),
    nes_mod_register_function_entry_plugin("smb3.wide-player-projectile", 0xA333, update_player_projectile),
    nes_mod_register_function_entry_plugin("smb3.wide-player-projectile-hit", 0xA68D, player_projectile_hit),
    nes_mod_register_function_entry_plugin("smb3.wide-cannon", 0xBB81, update_cannon),
    nes_mod_register_function_entry_plugin("smb3.wide-actor-draw-start", 0xC9C2, actor_draw_start),
    nes_mod_register_function_entry_plugin("smb3.wide-actor-draw-end", 0xC9C8, actor_draw_end),
    nes_mod_register_function_entry_plugin("smb3.wide-actor-retain", 0xD3EF, actor_keep_visible),
    nes_mod_register_function_entry_plugin("smb3.wide-box-origin", 0xD955, remember_collision_box),
    nes_mod_register_function_entry_plugin("smb3.wide-box-intersect", 0xD97B, intersect_world_boxes),
    nes_mod_register_function_entry_plugin("smb3.wide-shell-blocks", 0xCCA0, shell_block_boundary),
    nes_mod_register_function_entry_plugin("smb3.wide-player-crush", 0xA45C, player_crush_boundary),
    nes_mod_register_function_entry_plugin("smb3.wide-player-boundary", 0xB1D0, player_window_boundary),
    nes_mod_register_function_entry_plugin("smb3.wide-autoscroll", 0xBCC0, autoscroll_player),
    nes_mod_register_function_entry_plugin("smb3.coop-player", smb3::CoopPlayerUpdate, coop_player_hook),
    nes_mod_register_function_entry_plugin("smb3.coop-player-done", 0xA1AB, coop_player_done_hook),
    nes_mod_register_function_entry_plugin("smb3.coop-scrolling", 0xB11F, coop_scrolling_hook),
    nes_mod_register_function_entry_plugin("smb3.coop-die", 0xDA7C, coop_die_hook),
    nes_mod_register_function_entry_plugin("smb3.coop-throw", 0xA247, coop_throw_hook),
    nes_mod_register_function_entry_plugin("smb3.coop-special-done", 0xAF20, coop_special_done_hook),
    nes_mod_register_function_entry_plugin("smb3.coop-block-flower", smb3::CoopBlockFlower, coop_skip_mushroom_hook),
    nes_mod_register_function_entry_plugin("smb3.coop-block-leaf", smb3::CoopBlockLeaf, coop_skip_mushroom_hook),
    nes_mod_register_function_entry_plugin("smb3.coop-take-flower", smb3::CoopTakeFlower, coop_skip_mushroom_hook),
    nes_mod_register_function_entry_plugin("smb3.coop-take-leaf", smb3::CoopTakeLeaf, coop_skip_mushroom_hook),
    nes_mod_register_savestate_hook("smb3.coop", smb3::coop_save, smb3::coop_load),
#ifdef CYC_WITH_RECOMP_UI
    nes_mod_register_function_entry_plugin("smb3.practice-world", 0x84A0, practice_start_world),
    nes_mod_register_function_entry_plugin("smb3.practice-level", 0x8732, practice_enter_level),
#endif
};

void state_loaded(void *) {
#ifdef CYC_WITH_RECOMP_UI
    release_pause();
    paused_buttons = {};
#endif
    frontend.loaded();
#ifdef CYC_WITH_RECOMP_UI
    practice_target = nullptr;
#endif
    snapshot_valid = false;
    smb3::extra_sprites = {};
    smb3::displayed_extra_sprites = {};
    smb3::drawing_actor = -1;
    screen_effects = {};
    smb3::coop.pass = smb3::CoopPass::Idle; // States are taken between frames.
    smb3::coop.swapped = 0;
}

Image rgba_image(const unsigned char *rgba, int width, int height) {
    Image result;
    result.width = width;
    result.height = height;
    result.pixels.resize(static_cast<size_t>(width) * height);
    for (size_t i = 0; i < result.pixels.size(); ++i)
        result.pixels[i] = (uint32_t(rgba[i * 4 + 3]) << 24) |
                          (uint32_t(rgba[i * 4]) << 16) |
                          (uint32_t(rgba[i * 4 + 1]) << 8) | rgba[i * 4 + 2];
    return result;
}

// Reading via filesystem preserves non-ASCII paths on Windows.
FILE *open_file(const std::filesystem::path &path) {
#ifdef _WIN32
    return _wfopen(path.c_str(), L"rb");
#else
    return std::fopen(path.c_str(), "rb");
#endif
}

bool png_size(const std::filesystem::path &path, int &width, int &height) {
    FILE *file = open_file(path);
    if (!file) return false;
    int channels = 0;
    const bool valid = stbi_info_from_file(file, &width, &height, &channels);
    std::fclose(file);
    return valid;
}

Image load_image(const std::filesystem::path &path, int64_t max_pixels = 4194304) {
    FILE *file = open_file(path);
    if (!file) return {};
    int channels = 0;
    int width = 0, height = 0;
    if (!stbi_info_from_file(file, &width, &height, &channels) ||
        width <= 0 || height <= 0 || static_cast<int64_t>(width) * height > max_pixels) {
        std::fclose(file);
        return {};
    }
    unsigned char *rgba = stbi_load_from_file(file, &width, &height, &channels, 4);
    std::fclose(file);
    if (!rgba) return {};
    Image result = rgba_image(rgba, width, height);
    stbi_image_free(rgba);
    return result;
}

std::vector<uint8_t> read_file(const std::filesystem::path &path, size_t limit) {
    std::vector<uint8_t> data;
    FILE *file = open_file(path);
    if (!file) return data;
    data.resize(limit + 1);
    data.resize(std::fread(data.data(), 1, data.size(), file));
    std::fclose(file);
    if (data.size() > limit) data.clear();
    return data;
}

// The newer character sheet is published as WebP. Only that exact size is decoded.
Image load_webp(const std::filesystem::path &path, int width, int height) {
    const auto data = read_file(path, 16 * 1024 * 1024);
    int actual_width = 0, actual_height = 0;
    if (data.empty() || !WebPGetInfo(data.data(), data.size(), &actual_width, &actual_height) ||
        actual_width != width || actual_height != height) return {};
    uint8_t *rgba = WebPDecodeRGBA(data.data(), data.size(), &actual_width, &actual_height);
    if (!rgba) return {};
    Image result = rgba_image(rgba, actual_width, actual_height);
    WebPFree(rgba);
    return result;
}

Image load_art(const std::string &name) {
    Image result;
    for (size_t i = 0; result.empty() && i < pack_sprites.size(); ++i) result = load_image(pack_sprites[i] / name);
    return result;
}

const Image &image(const std::string &name) {
    auto found = images.find(name);
    if (found == images.end()) found = images.emplace(name, load_art(name)).first;
    return found->second;
}

const Image &player_sheet(int index, const char *filename) {
    return image(std::string("Players/") + Characters[index] + "/" + filename);
}

smb3::Sprite player_sprite(int index, unsigned form, unsigned cell) {
    if (index < 2 || index > 3) return {};
    const char *const sheets[] = {"Small.png", "Big.png", "Fire.png"};
    const char *sheet = form >= 3 || (!form && cell >= 24) ? "Campaign.png" : sheets[form];
    return smb3::reference_sprite(player_sheet(index, sheet), form, cell);
}

std::array<uint32_t, 4> character_colors(int index, unsigned form) {
    if (form == 5) return {0, 0xffb86b22, 0xffffd9b2, 0xff1f1f1f};
    if (form == 2) {
        if (index == 1) return {0, 0xff4cd950, 0xffffd9b2, 0xffdd591f};
        return {0, index == 2 ? 0xff27c3f2u : 0xffff92d5u, 0xffffd9b2, 0xfff73910};
    }
    if (index == 1) return {0, 0xff28b835, 0xffffd9b2, 0xff1f1f1f};
    return {0, index == 2 ? 0xff007effu : 0xffff50c8u, 0xffffd9b2, 0xff1f1f1f};
}

bool any_missing(std::initializer_list<const char *> names) {
    for (const char *name : names)
        if (image(name).empty()) return true;
    return false;
}

void add_missing(smb3::NamedImages art) {
    for (auto &[name, picture] : art)
        if (image(name).empty()) images[name] = std::move(picture);
}

// Packs follow SMB1 Remastered: each folder in resource_packs/ keeps its art under Sprites/.
// The character sheets and Vryell's icons are converted here from the player's downloads.
// shortcut: ZIP downloads must be extracted first; read them in place if players keep missing that step.
void load_resource_packs(const std::filesystem::path &root) {
    std::error_code error;
    std::filesystem::create_directories(root, error); // Shows players where packs go.
    std::vector<std::filesystem::path> files;
    for (std::filesystem::recursive_directory_iterator entry(
             root, std::filesystem::directory_options::skip_permission_denied, error), end;
         !error && entry != end; entry.increment(error)) {
        if (!entry->is_directory(error)) files.push_back(entry->path());
        else if (entry->path().filename() == "Sprites") {
            pack_sprites.push_back(entry->path());
            entry.disable_recursion_pending();
        }
    }
    std::sort(pack_sprites.begin(), pack_sprites.end());
    std::sort(files.begin(), files.end());
    constexpr int64_t SheetPixels = 1 << 24;
    for (const auto &file : files) {
        const auto extension = file.extension();
        int width = 0, height = 0;
        const bool png = extension == ".png" && png_size(file, width, height);
        if (png && width == smb3::CampaignSheetWidth && height == smb3::CampaignSheetHeight) {
            if (any_missing({"Players/Luigi/Campaign.png", "Players/Toad/Campaign.png", "Players/Toadette/Campaign.png"}))
                add_missing(smb3::campaign_art(load_image(file, SheetPixels)));
        } else if ((png && width == smb3::ReferenceSheetWidth && height == smb3::ReferenceSheetHeight) ||
                   extension == ".webp") {
            if (any_missing({"Players/Mario/Selector.png", "Players/Luigi/Selector.png", "Players/Toad/Selector.png",
                             "Players/Toadette/Selector.png", "Players/Luigi/Map.png", "Players/Toad/Map.png",
                             "Players/Toadette/Map.png", "Players/Luigi/Airborne.png", "Players/Toad/Airborne.png",
                             "Players/Toadette/Airborne.png"}))
                add_missing(smb3::reference_art(png ? load_image(file, SheetPixels) :
                    load_webp(file, smb3::ReferenceSheetWidth, smb3::ReferenceSheetHeight)));
        } else if (png && file.filename() == "kb_dark_all.png" && any_missing({"Controls/Hints.png"})) {
            add_missing({{"Controls/Hints.png",
                          smb3::hint_atlas(load_image(file), load_image(file.parent_path() / "controller_xbox.png"))}});
        }
    }
}

void load_local_art() {
    if (art_loaded) return;
    art_loaded = true;
    images.clear(); // Drop lookups made before the packs were found.
    std::filesystem::path base = std::filesystem::current_path();
#ifdef CYC_WITH_SDL
    char *executable_path = SDL_GetBasePath();
    if (executable_path) { base = std::filesystem::u8path(executable_path); SDL_free(executable_path); }
#endif
    game_directory = base;
    load_resource_packs(base / "resource_packs");
    character_count = 4;
    // Form 3 reads Campaign.png, which Toad and Toadette need for every suit beyond Fire.
    for (int index = 2; index < 4; ++index)
        for (unsigned form = 0; form < 4; ++form)
            if (!smb3::sprite_visible(player_sprite(index, form, 0))) character_count = 2;
}

int view_mode(void *) { return view; }
bool set_view(void *, int value) {
    if (value < 0 || value > 2) return false;
    view = value;
    cyc_video_set_mode(value == 0 ? NES_VIDEO_STOCK : value == 1 ? NES_VIDEO_16_9 : NES_VIDEO_FIT);
    return true;
}

bool integer(const char *text, int minimum, int maximum, int &output) {
    if (!text || !*text) return false;
    char *end = nullptr;
    const long value = std::strtol(text, &end, 10);
    if (*end || value < minimum || value > maximum) return false;
    output = static_cast<int>(value);
    return true;
}

void load_setting(void *, const char *key, const char *value) {
    if (!std::strcmp(key, "Character")) integer(value, 0, 3, character);
    if (!std::strcmp(key, "Partner")) integer(value, 0, 3, partner);
    if (!std::strcmp(key, "Remastered")) integer(value, 0, 1, enhanced);
    if (!std::strcmp(key, "Shadows")) integer(value, 0, 1, shadows);
    if (!std::strcmp(key, "BackgroundParticles")) integer(value, 0, 1, particles);
    if (!std::strcmp(key, "Parallax")) integer(value, 0, 1, parallax);
    if (!std::strcmp(key, "ModernHud")) integer(value, 0, 1, modern_hud);
    if (!std::strcmp(key, "RainbowStar")) integer(value, 0, 1, rainbow_star);
    if (!std::strcmp(key, "PresentationAspect")) integer(value, 0, 2, view);
    if (!std::strcmp(key, "Language")) {
        if (!std::strcmp(value, "es")) spanish_ui = true;
        if (!std::strcmp(value, "en")) spanish_ui = false;
    }
}

void save_settings(void *, FILE *file) {
    std::fprintf(file, "Character = %d\nPartner = %d\nRemastered = %d\nShadows = %d\nBackgroundParticles = %d\n"
                 "Parallax = %d\nModernHud = %d\nRainbowStar = %d\nPresentationAspect = %d\nLanguage = %s\n",
                 character, partner, enhanced, shadows, particles, parallax, modern_hud, rainbow_star, view,
                 smb3_text("en", "es"));
}

bool option(void *, const char *name, const char *value) {
    if (!std::strcmp(name, "--skip-selection")) { frontend.loaded(); return true; }
    if (!std::strcmp(name, "--coop")) { smb3::coop.enabled = 1; return true; }
    if (!std::strcmp(name, "--level")) { requested_level = value; return true; }
    if (!std::strcmp(name, "--character") || !std::strcmp(name, "--partner")) {
        for (int i = 0; i < 4; ++i) {
            std::string candidate = Characters[i];
            for (char &letter : candidate) letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
            if (candidate != value) continue;
            load_local_art();
            if (i >= character_count) {
                std::fprintf(stderr, "%s necesita los resource packs descritos en README.md.\n", Characters[i]);
                return false;
            }
            (std::strcmp(name, "--partner") ? character : partner) = i;
            return true;
        }
    }
    if (!std::strcmp(name, "--graphics")) {
        if (!std::strcmp(value, "classic")) { enhanced = 0; return true; }
        if (!std::strcmp(value, "remastered")) { enhanced = 1; return true; }
    }
    if (!std::strcmp(name, "--aspect")) {
        if (!std::strcmp(value, "native")) return set_view(nullptr, 0);
        if (!std::strcmp(value, "16:9")) return set_view(nullptr, 1);
        if (!std::strcmp(value, "fit")) return set_view(nullptr, 2);
    }
    return false;
}

// banks: PatTable_BankSel's six registers, by default those of the frame shown.
uint8_t chr_byte(unsigned address, const uint8_t *banks = nullptr) {
    if (rom.size() != 393216) return 0;
    if (!banks) banks = (snapshot_valid ? frame_ram.data() : cyc_cpu_ram()) + 0x719;
    const unsigned bank = address < 0x1000 ?
        (banks[address / 2048] & 0x7e) + (address / 1024 & 1) :
        banks[2 + (address - 0x1000) / 1024];
    return rom[ChrStart + (bank & 127) * 1024 + (address & 1023)];
}

struct SpriteRow {
    uint8_t low = 0, high = 0;
    bool flipped = false;

    unsigned pixel(int x) const {
        const int shift = flipped ? x : 7 - x;
        return ((low >> shift) & 1) | (((high >> shift) & 1) << 1);
    }
};

SpriteRow sprite_row(const uint8_t *sprite, int y, const uint8_t *banks = nullptr) {
    const int top = sprite[0] + 1;
    const bool tall = cyc_render_line_sprite16(std::min(top, Height - 1));
    const int h = tall ? 16 : 8;
    if (y >= h) return {};
    if (sprite[2] & 0x80) y = h - 1 - y;
    const uint16_t pattern = tall ? (sprite[1] & 1) * 0x1000 +
        ((sprite[1] & 0xfe) + y / 8) * 16 :
        cyc_render_line_sprite_table(std::min(top, Height - 1)) + sprite[1] * 16;
    return {chr_byte(pattern + y % 8, banks), chr_byte(pattern + y % 8 + 8, banks), bool(sprite[2] & 0x40)};
}

// Player 2 in co-op: their own choice, never player 1's character.
int player2_character() {
    if (partner != character && partner < character_count) return partner;
    return character == 1 ? 0 : 1;
}
// Whose player variables are in RAM: player 1's, unless co-op handed the lead to player 2.
int lead_character() { return smb3::coop.enabled && smb3::coop.partner_is_p1 ? player2_character() : character; }
int partner_who() { return smb3::coop.partner_is_p1 ? character : player2_character(); }

// Small, Super and Raccoon share the red or green palette of the character wearing them.
uint32_t player_color(unsigned nes_color, unsigned suit, bool star, int who) {
    if (who < 2 && !star && (suit < 2 || suit == 3) && (nes_color == 0x16 || nes_color == 0x2a))
        nes_color = who == 0 ? 0x16 : 0x2a;
    return smb3::sprite_color(nes_color);
}
uint32_t player_color(unsigned nes_color, const uint8_t *ram) {
    return player_color(nes_color, ram[0xed], ram[0x553] || ram[0x555], lead_character());
}

uint8_t sprite_palette(unsigned index) {
    return snapshot_valid ? frame_palette[index] : cyc_render_palette(index);
}

void cloud(uint32_t *out, int width, int left, int top, int span, uint32_t sky) {
    const uint32_t light = smb3::blend(sky, 0x81fffdf6), shade = smb3::blend(sky, 0x52fffdf6);
    for (int y = 0; y < 17; ++y) {
        for (int dx = std::max(-left, 0); dx < std::min(span, width - left); ++dx) {
            const int x = dx * 48 / span;
            const bool inside = (x - 9) * (x - 9) + (y - 14) * (y - 14) < 8 * 8 ||
                (x - 21) * (x - 21) + (y - 11) * (y - 11) < 10 * 10 ||
                (x - 31) * (x - 31) + (y - 10) * (y - 10) < 8 * 8 ||
                (x - 40) * (x - 40) + (y - 15) * (y - 15) < 6 * 6;
            if (inside) out[(top + y) * width + left + dx] = y < 14 ? light : shade;
        }
    }
}

void backdrop_layer(uint32_t *out, int width, const Image &layer, int horizontal, int top, unsigned opacity,
                    int tick = 0, int frame_ticks = 0) {
    if (layer.empty()) return;
    const auto frame = smb3::backdrop_frame(layer, tick, frame_ticks);
    if (!frame.width) return;
    const int start = smb3::wrap(horizontal, frame.width);
    for (int y = std::max(0, -top); y < std::min(PlayfieldHeight, layer.height - top); ++y) {
        const uint32_t *row = layer.pixels.data() + (y + top) * layer.width + frame.left;
        int column = start;
        for (int x = 0; x < width; ++x) {
            const uint32_t pixel = row[column];
            if (++column == frame.width) column = 0;
            const uint32_t alpha = (pixel >> 24) * opacity / 255;
            if (alpha == 255) out[y * width + x] = pixel;
            else if (alpha) out[y * width + x] = smb3::blend(out[y * width + x], (pixel & 0xffffff) | alpha << 24);
        }
    }
}

// With parallax off, every scenery layer moves with the ground, like SMB1 Remastered's setting.
int layer_offset(int camera_left, int width, int divisor) {
    return smb3::parallax_offset(camera_left, width, parallax ? divisor : 1);
}

void ambient_particles(uint32_t *out, int width, int camera_left, int tick, smb3::Atmosphere atmosphere) {
    using smb3::Atmosphere;
    if (!particles || atmosphere == Atmosphere::Clear) return;
    const bool rising = atmosphere == Atmosphere::Bubbles || atmosphere == Atmosphere::Embers;
    const uint32_t color = atmosphere == Atmosphere::Bubbles ? 0x6299c7e5 :
        atmosphere == Atmosphere::Embers ? 0x64de9f73 : atmosphere == Atmosphere::Snow ? 0x91fffdf8 : 0x58ffe9cc;
    const int offset = layer_offset(camera_left, width, 5);
    constexpr int period = 384;
    const int count = atmosphere == Atmosphere::Snow ? 20 : 12;
    for (int i = 0; i < count; ++i) {
        const int phase = (tick / (3 + i % 3) + i * 37) % (PlayfieldHeight + 4);
        const int y = (rising ? PlayfieldHeight - phase : phase) - 2;
        if (y < 1 || y + 1 >= PlayfieldHeight) continue;
        const int drift = atmosphere == Atmosphere::Wind ? tick / 3 : ((phase / 3 + i) & 7) - 4;
        for (int x = smb3::wrap(i * 97 - offset + drift, period); x < width; x += period) {
            if (atmosphere == Atmosphere::Bubbles && x > 0 && x + 1 < width) {
                out[(y - 1) * width + x] = smb3::blend(out[(y - 1) * width + x], color);
                out[y * width + x - 1] = smb3::blend(out[y * width + x - 1], color);
                out[y * width + x + 1] = smb3::blend(out[y * width + x + 1], color);
                out[(y + 1) * width + x] = smb3::blend(out[(y + 1) * width + x], color);
            } else {
                out[y * width + x] = smb3::blend(out[y * width + x], color);
                if (atmosphere == Atmosphere::Wind && x + 1 < width)
                    out[y * width + x + 1] = smb3::blend(out[y * width + x + 1], color);
            }
        }
    }
}

void backdrop(uint32_t *out, int width, int camera_left, int camera_y, const smb3::Backdrop &style, int tick) {
    const int vertical = style.atmosphere == smb3::Atmosphere::Bubbles ? 0 : smb3::backdrop_height(camera_y) / 2;
    std::fill_n(out, width * PlayfieldHeight, style.base);
    std::fill_n(out + width * PlayfieldHeight, width * (Height - PlayfieldHeight), 0xff21343b);
    static const Image missing;
    const Image &sky = style.sky ? image(std::string("Backgrounds/") + style.sky) : missing;
    const int drift = style.fallback_clouds && !style.sky_frame_ticks ? tick / 15 : 0;
    const int sky_offset = layer_offset(camera_left, width, 10) + drift;
    const bool has_sky = smb3::backdrop_frame(sky, tick, style.sky_frame_ticks).width > 0;
    if (has_sky) {
        backdrop_layer(out, width, sky, sky_offset, style.sky_row, style.sky_opacity, tick, style.sky_frame_ticks);
    } else if (style.fallback_clouds) {
        constexpr int Period = 416;
        const int offset = smb3::wrap(sky_offset, Period);
        for (int x = -Period; x < width + Period; x += Period) {
            cloud(out, width, x + 54 - offset, 33, 56, style.base);
            cloud(out, width, x + 244 - offset, 75, 36, style.base);
        }
    } else if (style.sky_frame_ticks && style.atmosphere != smb3::Atmosphere::Bubbles) {
        constexpr int Period = 384;
        for (int i = 0; i < 15; ++i) {
            const int y = 12 + i * 23 % 109;
            const unsigned alpha = (tick / 24 + i) % 5 == 0 ? 120 : 55;
            const uint32_t star = smb3::blend(style.base, alpha << 24 | 0xfff1cc);
            for (int x = smb3::wrap(15 + i * 73 - sky_offset, Period); x < width; x += Period)
                out[y * width + x] = star;
        }
    }
    if (style.trees) {
        const Image &trees = image(std::string("Backgrounds/SecondaryTrees/") + style.trees + ".png");
        backdrop_layer(out, width, trees, layer_offset(camera_left, width, 6),
                       vertical / 2 + trees.height - PlayfieldHeight + 12, 46);
    }
    const Image &hills = style.hills ? image(std::string("Backgrounds/Hills/") + style.hills + ".png") : missing;
    const int hills_offset = layer_offset(camera_left, width, 4);
    if (!hills.empty()) {
        backdrop_layer(out, width, hills, hills_offset, vertical + hills.height - PlayfieldHeight + 8, style.hills_opacity);
    } else if (style.landform != smb3::Landform::None && (style.hills || !has_sky)) {
        static smb3::Backdrop previous{};
        static Image near, far;
        if (previous.landform != style.landform || previous.base != style.base || previous.horizon != style.horizon) {
            // Use the grassland metatile's repeating triangular texture from the supplied ROM.
            const auto *pattern = rom.size() == 393216 ? rom.data() + ChrStart + 8 * 1024 + 0x0f * 16 : nullptr;
            near = smb3::landscape_layer(style, pattern, false);
            far = smb3::landscape_layer(style, pattern, true);
            previous = style;
        }
        backdrop_layer(out, width, far, layer_offset(camera_left, width, 7),
                       vertical / 2 + far.height - PlayfieldHeight, 255);
        backdrop_layer(out, width, near, hills_offset, vertical + near.height - PlayfieldHeight, 255);
    }
    ambient_particles(out, width, camera_left, tick, style.atmosphere);
}

int camera_x(const uint8_t *ram) {
    const int ppu_x = cyc_render_line_scroll_x(80) & 255;
    const int next = ram[0x12] * 256 + ram[0xfd];
    // SMB3 streams horizontal columns into a 256-pixel ring, not a 512-pixel world page.
    return smb3::unwrap(ppu_x, next);
}

unsigned scenery_light(const uint8_t *ram) {
    const unsigned step = std::min<unsigned>(ram[0x41d], 4);
    if (ram[0x41b] == 1) return (4 - step) * 255 / 4;
    if (ram[0x41b] == 3) return step * 255 / 4;
    return 255;
}

int level_tile_address(int x, int y) {
    return (x / NativeWidth) * LevelScreenTiles + (y / 16) * LevelColumns + (x % NativeWidth) / 16;
}

int goal_edge(const uint8_t *cart, int goal_x) {
    // The level loader writes the zigzag as a vertical column of metatile 1.
    for (int x = goal_x; x >= std::max(0, goal_x - NativeWidth); x -= 16) {
        const int address = level_tile_address(x, 0);
        if (cart[address] == 1 && cart[address + LevelColumns] == 1 &&
            cart[address + 2 * LevelColumns] == 1) return x;
    }
    return -1;
}

struct TileFrames {
    const Image &question;
    const Image &brick;
    const Image &coin;
    int left;
    int blocks_top;
    int coin_top;
    unsigned light;
};

struct MetatileFrame {
    std::array<uint32_t, 16 * 16> pixels, water;
};

// The 8 x 8 pattern the room data places at this point, as the level loader writes it to the nametable.
unsigned terrain_pattern(unsigned tile, int tileset, int wx, int wy) {
    static constexpr int banks[] = {11,15,21,16,17,19,18,18,18,20,23,19,17,19,13};
    const unsigned quadrant = ((wx & 15) / 8) * 2 + ((wy & 15) / 8);
    return rom[banks[tileset] * 8192 + quadrant * 256 + tile];
}

unsigned terrain_texel(unsigned tile, int tileset, const uint8_t *chr_banks, int wx, int wy) {
    const unsigned pattern = terrain_pattern(tile, tileset, wx, wy);
    const unsigned bank = chr_banks[pattern >= 128] & 0x7e;
    const uint8_t *chr = rom.data() + ChrStart + bank * 1024 + (pattern & 127) * 16;
    return smb3::tile_pixel(chr, wx & 7, wy & 7);
}

uint32_t terrain_pixel(unsigned tile, unsigned value, int tileset, int wx, int wy,
                       bool goal_backdrop, const TileFrames &frames) {
    // The ice sky and decorative clouds have opaque stripes baked into their tiles.
    if (tileset == 12 && (tile == 0x80 || tile == 0x86 || (tile >= 0x08 && tile <= 0x0d))) return 0;
    const smb3::TileAnimation animation = smb3::tile_animation(tile);
    const bool coin = animation == smb3::TileAnimation::Coin;
    const Image *sheet = animation == smb3::TileAnimation::Question ? &frames.question :
        animation == smb3::TileAnimation::Brick ? &frames.brick : coin ? &frames.coin : nullptr;
    if (sheet && !sheet->empty()) {
        const int top = coin ? frames.coin_top : frames.blocks_top;
        const uint32_t pixel = sheet->at(frames.left + (wx & 15), top + (wy & 15));
        if (coin || pixel >> 24) return smb3::dim(pixel, frames.light);
    }
    if (!value) return 0;
    const int index = (tile >> 6) * 4 + value;
    const uint8_t color = snapshot_valid ? frame_palette[index] : cyc_render_palette(index);
    if (goal_backdrop && index < 4 && color == 0x0f) return GoalBlack;
    if (frames.light < 255 || !enhanced) return smb3::color(color);
    return smb3::animated_tile_color(animation, color, value, wx & 15, wy & 15, frames.left / 16);
}

struct PlayerBounds {
    int left = 4096, top = 240, right = -4096, bottom = -1, first = 0;
    bool visible() const { return right >= left && bottom >= top; }
};

// One player's six sprites as its Player_Draw left them, placed on the wide screen.
struct PlayerTiles {
    const uint8_t *oam = nullptr;          // Six sprites of four bytes
    const uint8_t *banks = nullptr;        // PatTable_BankSel when they were drawn; nullptr: the frame's
    const uint8_t *suit_colors = nullptr;  // Palette 0 from PowerUp_Palettes; nullptr: the PPU's
    std::array<int, 6> left{};             // Screen X of each sprite
    int rise = 0;                          // Vertical scroll since they were drawn
    int top(int slot) const { return oam[slot * 4] + 1 + rise; }
};

PlayerBounds tile_bounds(const PlayerTiles &tiles) {
    PlayerBounds bounds;
    for (int slot = 0; slot < 6; ++slot) {
        const int top = tiles.top(slot), left = tiles.left[slot];
        if (top >= PlayfieldHeight) continue;
        for (int y = 0; y < 16 && y + top < PlayfieldHeight; ++y) {
            const auto row = sprite_row(tiles.oam + slot * 4, y, tiles.banks);
            for (int x = 0; x < 8; ++x)
                if (row.pixel(x)) {
                    bounds.left = std::min(bounds.left, left + x);
                    bounds.top = std::min(bounds.top, top + y);
                    bounds.right = std::max(bounds.right, left + x);
                    bounds.bottom = std::max(bounds.bottom, top + y);
                }
        }
    }
    return bounds;
}

// SMB3 keeps these effects at an eight-bit X relative to the NES screen, so past its 256 pixels they
// wrapped to the other side. Every emulated frame, a new effect is placed at the copy next to what made
// it: the block the game just changed, the actor that hit or was hit, a cannon or the player. It then
// keeps that copy while it moves, and its sprites are drawn there.
void update_screen_effects() {
    ++effect_tick;
    const uint8_t *ram = cyc_cpu_ram();
    size_t cart_size = 0;
    const uint8_t *cart = cyc_cart_ram(&cart_size);
    if (!smb3::wide_level(ram, true) || !cart || cart_size < 0x1fc6) return;
    const int scroll = ram[0x12] * 256 + ram[0xfd], scroll_y = ram[0x543]; // Horz_Scroll, Level_VertScroll
    const std::vector<int> block{ram[0x528] * 256 + ram[0x529] - scroll}, player{ram[0x75] * 256 + ram[0x90] - scroll};
    std::vector<int> defeated, hitters = player, actors = player, cannons;
    for (int i = 0; i < 8; ++i) {
        const int state = ram[0x661 + i], x = ram[0x76 + i] * 256 + ram[0x91 + i] - scroll;
        if (state) actors.push_back(x);
        if (state == 3 || state >= 6) defeated.push_back(x); // Shelled, killed, squashed or poofed
        if (state == 4 || state == 5) hitters.push_back(x);  // Held or kicked
        if (cart[0x1a15 + i]) cannons.push_back(cart[0x1a2d + i] * 256 + cart[0x1a35 + i] - scroll);
    }
    std::vector<int> effects;
    for (const auto &effect : screen_effects)
        if (effect_tick - effect.seen <= 1) effects.push_back(effect.real);
    using Origins = std::initializer_list<std::pair<const std::vector<int> *, int>>;
    auto place = [&](int x8, Origins origins) {
        for (const auto &[list, tolerance] : origins) { // In priority order, each within its tolerance.
            int best = 0, distance = tolerance + 1;
            for (int origin : *list) {
                const int x = smb3::unwrap(x8, origin);
                if (std::abs(x - origin) < distance) { distance = std::abs(x - origin); best = x; }
            }
            if (distance <= tolerance) return best;
        }
        // Only an effect whose origin vanished the same frame gets here.
        actors.insert(actors.end(), effects.begin(), effects.end());
        const int left = smb3::world_left(ram) - scroll;
        return smb3::wrapped_effect_x(x8, actors, left - 32, left + cyc_video_width() + 32);
    };
    auto track = [&](int index, bool active, int x8, int y, int spread, int tile, Origins origins) {
        auto &effect = screen_effects[index];
        if (!active) return;
        const int step = smb3::unwrap(x8, effect.x8) - effect.x8;
        // Effects move a few pixels per frame; a jump means the slot holds a new one.
        effect.real = effect_tick - effect.seen <= 1 && std::abs(step) <= 24 ? effect.real + step : place(x8, origins);
        effect = {x8, y, spread, tile, effect.real, effect_tick};
    };
    track(0, cart[0x1a46], (cart[0x1a48] - scroll) & 255, (cart[0x1a47] - scroll_y) & 255, 0, 0x57,
          {{&defeated, 16}, {&player, 40}}); // ShellKillFlash
    for (int i = 0; i < 5; ++i) // Scores
        track(1 + i, ram[0x79e + i], ram[0x7ad + i], ram[0x7a8 + i], 0, -1,
              {{&defeated, 8}, {&effects, 8}, {&actors, 8}, {&player, 32}});
    for (int i = 0; i < 4; ++i) // CoinPUp
        track(6 + i, cart[0x1fb2 + i], cart[0x1fba + i], cart[0x1fb6 + i], 0, -1, {{&block, 12}, {&hitters, 32}});
    for (int i = 0; i < 2; ++i) { // BrickBust: debris, or a poof at its upper Y
        const bool debris = cart[0x1f9a + i] == 2;
        track(10 + i, cart[0x1f9a + i], cart[0x1fa0 + i], debris ? -1 : cart[0x1f9d + i], cart[0x1fa6 + i],
              debris ? 0x4b : -1, {{&block, 12}, {&cannons, 32}, {&defeated, 24}, {&actors, 24}});
    }
    for (int i = 0; i < 3; ++i) // Splash, which can stay fixed on screen vertically
        track(12 + i, cart[0x1f8e + i], (cart[0x1f94 + i] - scroll) & 255,
              (cart[0x1f91 + i] - (cart[0x1f97 + i] ? 0 : scroll_y)) & 255, 0, -1, {{&player, 24}, {&actors, 24}});
}

void place_screen_effects(std::array<int, 64> &positions, std::array<bool, 64> &grouped) {
    const uint8_t *sprites = oam();
    for (int slot = 0; slot < 64; ++slot) {
        const uint8_t *sprite = sprites + slot * 4;
        if (grouped[slot] || sprite[0] >= 0xef) continue;
        for (const auto &effect : screen_effects) {
            // The sprites can be a frame behind the effect, so allow a few pixels of travel.
            const int dx = smb3::unwrap(sprite[3], effect.x8) - effect.x8;
            if (effect_tick - effect.seen > 1 || dx < -effect.spread - 4 || dx > effect.spread + 12 ||
                (effect.y >= 0 && std::abs(sprite[0] - effect.y) > 4) || (effect.tile >= 0 && sprite[1] != effect.tile))
                continue;
            positions[slot] = effect.real + dx;
            grouped[slot] = true;
            break;
        }
    }
}

// The character art that takes the place of one player's tiles; empty when the ROM's tiles stay.
struct PlayerArt {
    smb3::Sprite picture{};
    PlayerBounds bounds;
    int x = 0, y = 0;
    uint8_t attributes = 0;
    bool flip = false, upside_down = false, rom_tiles = false, poof = false, shoe = false;
    bool visible() const { return smb3::sprite_visible(picture); }
};

// ram: the player's own variables (the co-op partner's are swapped into a copy).
PlayerArt player_art(const uint8_t *ram, int who, const PlayerTiles &tiles) {
    PlayerArt art;
    const PlayerBounds player = art.bounds = tile_bounds(tiles);
    const unsigned frame = ram[0xee];
    const bool spinning = frame >= 0x1a && frame <= 0x1d;
    const bool shoe = art.shoe = (frame == 0x43 || frame == 0x18) && ram[0x577];
    const bool poof = art.poof = player.visible() && smb3::player_poof(tiles.oam);
    const unsigned form = smb3::reference_form(ram[0xed], frame);
    const bool water = ram[0x575] != 0;
    smb3::Sprite &reference = art.picture;
    if (who != 0 && player.visible() && !poof && frame != 0x1e) {
        const bool airborne = ram[0xd8] != 0;
        const bool attacking = (form == 3 || form == 5) ? ram[0x517] != 0 : ram[0x513] != 0;
        const auto pose = shoe ? smb3::AirbornePose::None :
            smb3::airborne_pose(form, frame, airborne, water, ram[0xcf] & 0x80, attacking);
        if (pose != smb3::AirbornePose::None)
            reference = smb3::airborne_sprite(player_sheet(who, "Airborne.png"), form, pose);
        // Older atlases reuse grounded poses in the six flight frames. The ROM still has those poses.
        const bool wing_pose = pose >= smb3::AirbornePose::Fly1 && pose <= smb3::AirbornePose::Flutter3;
        if (!wing_pose && !smb3::sprite_visible(reference)) {
            const bool facing_front = ram[0x571] || ram[0x558]; // Level_PipeMove or Player_PipeFace
            const unsigned atlas_frame = smb3::campaign_frame(form, frame, facing_front);
            if (who == 1 || smb3::campaign_pose(form, frame, water))
                reference = smb3::campaign_sprite(player_sheet(who, "Campaign.png"), form, atlas_frame);
            if (!shoe && !smb3::sprite_visible(reference))
                reference = player_sprite(who, form, smb3::reference_cell(form, frame, ram[0xbd] != 0, water));
        }
    }
    const uint8_t player_attr = tiles.oam[2];
    art.attributes = shoe ? tiles.oam[3 * 4 + 2] : player_attr;
    art.flip = !(art.attributes & 0x40);
    const int lower_row = tiles.top(0);
    art.x = tiles.left[0] - 8;
    art.y = shoe ? player.bottom + 1 - 32 : lower_row < PlayfieldHeight ? lower_row - 16 : tiles.top(3);
    if (who == 1 && player.visible() && !smb3::sprite_visible(reference) && !poof &&
        !spinning && !shoe && frame != 0x1e && player.right - player.left < 28 &&
        player.bottom - player.top < 32) {
        art.x = player.left - 2;
        art.y = player.bottom + 1 - 32;
        for (int slot = 0; slot < 6; ++slot) {
            const int x0 = tiles.left[slot] - art.x;
            const int y0 = tiles.top(slot) - art.y;
            for (int y = std::max(0, -y0); y < 16 && y0 + y < 32; ++y) {
                const auto row = sprite_row(tiles.oam + slot * 4, y, tiles.banks);
                for (int x = std::max(0, -x0); x < 8 && x0 + x < 32; ++x) {
                    const unsigned pixel = row.pixel(x);
                    if (pixel) reference[(y0 + y) * 32 + x0 + x] = pixel;
                }
            }
        }
        reference = smb3::luigi_silhouette(reference);
        art.rom_tiles = true;
        art.flip = false;
    }
    if (art.visible() && spinning) {
        int left = 32, top = 32, right = 0, bottom = 0;
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x)
                if (reference[y * 32 + x]) {
                    left = std::min(left, x); right = std::max(right, x);
                    top = std::min(top, y); bottom = std::max(bottom, y);
                }
        const int pivot_x = (left + right + 1) / 2, pivot_y = (top + bottom + 1) / 2;
        art.upside_down = player_attr & 0x80;
        art.x = (player.left + player.right + 1) / 2 - (art.flip ? 32 - pivot_x : pivot_x);
        art.y = (player.top + player.bottom + 1) / 2 - (art.upside_down ? 32 - pivot_y : pivot_y);
    }
    return art;
}

unsigned tile_nes_color(const PlayerTiles &tiles, unsigned attributes, unsigned value) {
    const unsigned palette = attributes & 3;
    return !palette && tiles.suit_colors ? tiles.suit_colors[value] : sprite_palette(16 + palette * 4 + value);
}

void paint_player(const PlayerArt &art, const PlayerTiles &tiles, const uint8_t *ram, int who, int width,
                  const std::vector<uint8_t> &opaque) {
    const auto colors = character_colors(who, smb3::reference_form(ram[0xed], ram[0xee]));
    const unsigned light = scenery_light(ram);
    const bool star = ram[0x553] || ram[0x555];
    const bool rainbow = rainbow_star && ram[0x553]; // Player_StarInv
    const int rainbow_phase = static_cast<int>((cyc_cycle_count() / 29781) & 0xffffff) * 9;
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) {
            const int px = art.x + x, py = art.y + y;
            if (px < 0 || px >= width || py < 0 || py >= PlayfieldHeight || !(cyc_render_line_mask(py) & 0x10)) continue;
            const unsigned role = art.picture[(art.upside_down ? 31 - y : y) * 32 + (art.flip ? 31 - x : x)];
            if (!role) continue;
            const unsigned nes = tile_nes_color(tiles, art.attributes, role);
            const uint32_t rom = player_color(nes, ram[0xed], star, who);
            const uint32_t color = rainbow ? smb3::rainbow(art.rom_tiles ? rom : colors[role], rainbow_phase + py * 6) :
                art.rom_tiles ? rom : star ? smb3::sprite_color(nes) : smb3::dim(colors[role], light);
            const int i = py * width + px;
            sprite_layer[i] = smb3::sprite_priority(sprite_layer[i], color, art.attributes & 0x20, opaque[i]);
        }
}

// The ROM's own tiles of a player drawn apart from the frame's OAM: the co-op partner as Mario, or
// the first three, its Kuribo's shoe, in their own colors.
void draw_player_tiles(const PlayerTiles &tiles, const uint8_t *ram, int who, int width,
                       const std::vector<uint8_t> &opaque, int count = 6) {
    const bool star = ram[0x553] || ram[0x555];
    for (int slot = count - 1; slot >= 0; --slot) {
        const uint8_t *sprite = tiles.oam + slot * 4;
        if (sprite[0] >= 0xef) continue;
        const int top = tiles.top(slot);
        const int h = cyc_render_line_sprite16(std::clamp(top, 0, Height - 1)) ? 16 : 8;
        for (int y = 0; y < h; ++y) {
            const int py = top + y;
            if (py < 0 || py >= PlayfieldHeight || !(cyc_render_line_mask(py) & 0x10)) continue;
            const auto row = sprite_row(sprite, y, tiles.banks);
            for (int x = 0; x < 8; ++x) {
                const int px = tiles.left[slot] + x;
                const unsigned value = row.pixel(x);
                if (!value || px < 0 || px >= width) continue;
                const int i = py * width + px;
                const unsigned nes = tile_nes_color(tiles, sprite[2], value);
                const uint32_t color = count == 6 ? player_color(nes, ram[0xed], star, who) : smb3::sprite_color(nes);
                sprite_layer[i] = smb3::sprite_priority(sprite_layer[i], color, sprite[2] & 0x20, opaque[i]);
            }
        }
    }
}

// A character standing, for menus, ghosts and bubbles: Selector.png, or its Super form.
std::array<uint32_t, 32 * 32> character_portrait(int index, bool peace = false) {
    std::array<uint32_t, 32 * 32> result{};
    const auto &poses = player_sheet(index, "Selector.png");
    if (poses.width == 64 && poses.height == 32) {
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x) {
                const uint32_t pixel = poses.at(x + (peace ? 32 : 0), y);
                result[y * 32 + x] = pixel >> 24 ? pixel | 0xff000000 : 0;
            }
        return result;
    }
    smb3::Sprite pixels = original_player;
    if (index == 1) {
        pixels = smb3::campaign_sprite(player_sheet(index, "Campaign.png"), 1, 0x0e);
        if (!smb3::sprite_visible(pixels)) pixels = smb3::luigi_silhouette(original_player);
    }
    if (index >= 2) pixels = player_sprite(index, 1, 0);
    const auto palette = index != 0 ? character_colors(index, 1) : std::array<uint32_t, 4>{
        0, cyc_render_color_value(0x16), cyc_render_color_value(0x36), cyc_render_color_value(0x0f)};
    for (int i = 0; i < 32 * 32; ++i) result[i] = pixels[i] ? palette[pixels[i]] | 0xff000000 : 0;
    return result;
}

// A co-op player who is away: a ghost the lead can touch, or a bubble floating back to the lead.
void draw_away_partner(uint32_t *out, int width, int origin, int camera, const uint8_t *ram) {
    const auto &coop = smb3::coop;
    if (coop.ghost && coop.ghost < 90 && (coop.ghost & 4)) return; // Blinks before it floats away.
    const auto picture = character_portrait(partner_who());
    const int bob = static_cast<int>(std::lround(2 * std::sin(effect_tick * 0.1)));
    const int center_x = origin + coop.away_x - camera;
    const int center_y = coop.away_y - (ram[0x542] * 256 + ram[0x543]) + bob; // Level_VertScrollH, Level_VertScroll
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) {
            const uint32_t pixel = picture[y * 32 + x];
            const int px = center_x - 16 + x, py = center_y - 16 + y;
            if (!pixel || px < 0 || px >= width || py < 0 || py >= PlayfieldHeight) continue;
            uint32_t &target = out[py * width + px];
            target = coop.ghost ? smb3::blend(target, (pixel & 0xffffff) | 0x90000000) : pixel;
        }
    if (!coop.bubble) return;
    for (int dy = -17; dy <= 17; ++dy)
        for (int dx = -17; dx <= 17; ++dx) {
            const int distance = dx * dx + dy * dy, px = center_x + dx, py = center_y + dy;
            if (distance > 17 * 17 || px < 0 || px >= width || py < 0 || py >= PlayfieldHeight) continue;
            const bool shine = (dx + 8) * (dx + 8) + (dy + 8) * (dy + 8) <= 6;
            uint32_t &target = out[py * width + px];
            target = smb3::blend(target, shine ? 0xe0ffffffu : distance >= 15 * 15 ? 0xc8e8f8ffu : 0x3090d0ffu);
        }
}

// The crown marks the player the camera follows, as in Super Mario Bros. Wonder.
void draw_crown(uint32_t *out, int width, const PlayerBounds &lead) {
    static constexpr const char *const crown[] = {"g..g..g", "gg.g.gg", "ggggggg", "grgbgrg", "ggggggg"};
    const int left = (lead.left + lead.right + 1) / 2 - 3, top = lead.top - 8;
    auto paint = [&](int x, int y, uint32_t color) {
        if (x >= 0 && x < width && y >= 0 && y < PlayfieldHeight) out[y * width + x] = color;
    };
    for (int y = -1; y <= 5; ++y)
        for (int x = -1; x <= 7; ++x) {
            auto at = [&](int cx, int cy) { return cx >= 0 && cx < 7 && cy >= 0 && cy < 5 && crown[cy][cx] != '.'; };
            if (!at(x, y) && (at(x - 1, y) || at(x + 1, y) || at(x, y - 1) || at(x, y + 1)))
                paint(left + x, top + y, 0xff2a1606);
        }
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 7; ++x)
            if (crown[y][x] != '.')
                paint(left + x, top + y, crown[y][x] == 'r' ? 0xffe8402c : crown[y][x] == 'b' ? 0xff4c8cf8 : 0xfff8c838);
}

void draw_sprites(uint32_t *out, int width, int origin, const uint8_t *ram,
                  const std::vector<uint8_t> &opaque) {
    const int character = lead_character(); // The player in RAM, whose sprites are in OAM.
    auto &sprites = sprite_layer;
    sprites.assign(width * PlayfieldHeight, 0);
    const uint8_t *sprites_oam = oam();
    const int first = ram[0x58e] / 4; // Player_SprOff
    const int camera = ram[0x3ef] ? 0 : camera_x(ram);
    const int player_x = smb3::unwrap(sprites_oam[first * 4 + 3], ram[0x75] * 256 + ram[0x90] - camera);
    std::array<int, 64> positions;
    std::array<bool, 64> grouped{};
    for (int slot = 0; slot < 64; ++slot) positions[slot] = sprites_oam[slot * 4 + 3];
    for (int slot = first; slot < std::min(first + 6, 64); ++slot) {
        positions[slot] = smb3::unwrap(positions[slot], player_x);
        grouped[slot] = true;
    }
    for (int object = 0; object < 8; ++object) {
        if (!ram[0x661 + object]) continue;
        const int object_first = ram[0x58f + object] / 4;
        int end = std::min(object_first + 8, 64);
        if (first > object_first) end = std::min(end, first);
        for (int other = 0; other < 8; ++other)
            if (ram[0x661 + other] && ram[0x58f + other] / 4 > object_first)
                end = std::min(end, ram[0x58f + other] / 4);
        const int expected = ram[0x76 + object] * 256 + ram[0x91 + object] - camera;
        const int anchor = smb3::unwrap(ram[0xac + object], expected);
        for (int slot = object_first; slot < end; ++slot) {
            if (grouped[slot] || std::abs(smb3::unwrap(positions[slot], ram[0xac + object]) - ram[0xac + object]) > 32) continue;
            positions[slot] = smb3::unwrap(positions[slot], anchor);
            grouped[slot] = true;
        }
    }
    for (int slot = 0; slot < 64; ++slot) {
        const auto &extra = smb3::displayed_extra_sprites[slot];
        if (width > 256 && !(slot >= first && slot < first + 6) &&
            extra.y == sprites_oam[slot * 4] && extra.x == sprites_oam[slot * 4 + 3]) {
            positions[slot] = extra.world_x - camera;
            grouped[slot] = true;
        }
    }
    if (width > 256 && !ram[0x3ef])
        place_screen_effects(positions, grouped);
    PlayerTiles lead_tiles{sprites_oam + std::min(first, 58) * 4};
    for (int slot = 0; slot < 6; ++slot) lead_tiles.left[slot] = origin + positions[std::min(first, 58) + slot];
    const PlayerArt lead = first <= 58 ? player_art(ram, character, lead_tiles) : PlayerArt{};
    const bool replacement = lead.visible();
    const bool rainbow = rainbow_star && ram[0x553]; // Player_StarInv
    const int rainbow_phase = static_cast<int>((cyc_cycle_count() / 29781) & 0xffffff) * 9;
    // The co-op partner, from the sprites its own Player_Update drew, behind everything else.
    // Its RAM view and suit palette are its own; the PPU's palette 0 follows the lead's suit.
    std::array<uint8_t, 2048> partner_ram;
    if (smb3::coop.enabled && smb3::coop_partner_active()) {
        const auto &coop = smb3::coop;
        std::copy_n(ram, partner_ram.size(), partner_ram.begin());
        for (size_t i = 0; i < smb3::CoopRamSize; ++i) partner_ram[smb3::CoopRam[i]] = coop.partner[i];
        const int suit = coop.partner[smb3::Suit];
        PlayerTiles tiles{coop.sprites.data(), coop.banks.data(), smb3::coop_palettes ?
            smb3::coop_palettes + (suit < 2 || suit == 3 ? 0 : std::min(suit, 7)) * 4 : nullptr};
        tiles.rise = coop.scroll_y - ram[0x543]; // Level_VertScroll then and now
        const int relative_x = smb3::coop_x(coop.partner.data()) - coop.scroll_x;
        for (int slot = 0; slot < 6; ++slot)
            tiles.left[slot] = origin + coop.scroll_x + smb3::unwrap(coop.sprites[slot * 4 + 3], relative_x) - camera;
        const int who = partner_who();
        const PlayerArt art = player_art(partner_ram.data(), who, tiles);
        if (!art.visible()) draw_player_tiles(tiles, partner_ram.data(), who, width, opaque);
        else {
            paint_player(art, tiles, partner_ram.data(), who, width, opaque);
            if (art.shoe) draw_player_tiles(tiles, partner_ram.data(), who, width, opaque, 3);
        }
    }
    // The ROM still controls timing, visibility and priority for every character.
    for (int slot = 63; slot >= 0; --slot) {
        const uint8_t *sprite = sprites_oam + slot * 4;
        const int top = sprite[0] + 1, left = origin + positions[slot];
        const bool is_player = !lead.poof && slot >= first + (lead.shoe ? 3 : 0) && slot < first + 6;
        if (is_player && replacement) {
            if (slot == first + 5) paint_player(lead, lead_tiles, ram, character, width, opaque);
            continue;
        }
        const int h = cyc_render_line_sprite16(std::min(top, Height - 1)) ? 16 : 8;
        for (int y = 0; y < h && y + top < PlayfieldHeight; ++y) {
            const int py = top + y;
            if (!(cyc_render_line_mask(py) & 0x10)) continue;
            const auto row = sprite_row(sprite, y);
            for (int x = 0; x < 8 && x + left < width; ++x) {
                const int px = left + x;
                if (px < 0) continue;
                if (!grouped[slot] && sprite[3] + x >= NativeWidth) continue;
                const unsigned value = row.pixel(x);
                const int i = py * width + px;
                if (!value) continue;
                const int palette_index = 16 + (sprite[2] & 3) * 4 + value;
                const unsigned color = sprite_palette(palette_index);
                uint32_t pixel = is_player ? player_color(color, ram) : smb3::sprite_color(color);
                if (is_player && rainbow) pixel = smb3::rainbow(pixel, rainbow_phase + py * 6);
                sprites[i] = smb3::sprite_priority(sprites[i], pixel, sprite[2] & 0x20, opaque[i]);
            }
        }
    }
    if (shadows) smb3::shadow(out, width, PlayfieldHeight, sprites, 3, 4, opaque.data());
    for (size_t i = 0; i < sprites.size(); ++i)
        if (sprites[i]) out[i] = sprites[i];
    if (smb3::coop.enabled && smb3::coop.present) {
        if (!smb3::coop_partner_active()) draw_away_partner(out, width, origin, camera, ram);
        if (lead.bounds.visible() && !ram[0xf1]) draw_crown(out, width, lead.bounds);
    }
}

uint32_t recolor(uint32_t pixel, bool sprite = false) {
    const auto found = frame_colors.find(pixel);
    if (found == frame_colors.end()) return pixel;
    return sprite ? smb3::sprite_color(found->second) : smb3::color(found->second);
}

bool inventory_visible(const uint8_t *ram) {
    return ram[0x5f2] || ram[0x3e6]; // Inventory_Open or InvFlip_Counter
}

constexpr uint32_t HudBand = 0xff111311;

// The status bar's NES black takes the band's black, so the panels sit on one dark strip.
uint32_t hud_color(uint32_t native_pixel) {
    const uint32_t pixel = recolor(native_pixel);
    return pixel == smb3::color(0x0f) ? HudBand : pixel;
}

// Player 1's badge shows the chosen character's initial in the 6 x 5 box where the ROM draws <M>.
// The letter is only replaced while the bar shows that exact M, so the flip animation is untouched.
void draw_badge(uint32_t *out, int width, int left, const uint32_t *native, const uint8_t *ram) {
    if (!character || (ram[0x726] & 1)) return; // Player 2 keeps the ROM's <L>.
    static constexpr const char *const mario[] = {"o...oo", "oo.ooo", "oooooo", "o.o.oo", "o...oo"};
    auto shows_mario = [&](int top) {
        for (int y = 0; y < 5; ++y)
            for (int x = 0; x < 6; ++x) {
                const uint32_t pixel = recolor(native[(top + y) * NativeWidth + 21 + x]);
                const bool white = pixel == smb3::color(0x30) || pixel == smb3::color(0x20);
                if (white != (mario[y][x] == 'o')) return false;
            }
        return true;
    };
    // The map's status bar sits one row higher than the levels'.
    const int top = shows_mario(212) ? 212 : shows_mario(211) ? 211 : 0;
    if (!top) return;
    static constexpr const char *const letters[][5] = {
        {"oo....", "oo....", "oo....", "oo....", "oooooo"},
        {"oooooo", "..oo..", "..oo..", "..oo..", "..oo.."}};
    static constexpr uint32_t colors[] = {0, 0xffb4e987, 0xffb6dcfc, 0xfffad5ee};
    const auto &letter = letters[character == 1 ? 0 : 1];
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 6; ++x)
            out[(top + y) * width + left + 21 + x] = letter[y][x] == 'o' ? colors[character] : HudBand;
}

void draw_hud(uint32_t *out, int width, int origin, const uint32_t *native, bool inventory, const uint8_t *ram) {
    if (inventory) {
        // The flip and the chest reward both travel across a single continuous panel.
        for (int y = PlayfieldHeight; y < Height; ++y) {
            const uint32_t *source = native + y * NativeWidth;
            uint32_t *row = out + y * width;
            std::fill_n(row, origin, hud_color(source[0]));
            for (int x = 0; x < NativeWidth; ++x) row[origin + x] = hud_color(source[x]);
            std::fill_n(row + origin + NativeWidth, width - origin - NativeWidth, hud_color(source[NativeWidth - 1]));
        }
        draw_badge(out, width, origin, native, ram);
        return;
    }
    if (!modern_hud) { // The NES status bar in one piece, centered on the band.
        std::fill_n(out + PlayfieldHeight * width, (Height - PlayfieldHeight) * width, HudBand);
        for (int y = PlayfieldHeight; y < Height; ++y)
            for (int x = 8; x < NativeWidth; ++x) out[y * width + origin + x] = hud_color(native[y * NativeWidth + x]);
        draw_badge(out, width, origin, native, ram);
        return;
    }
    const int margin = width >= 320 ? 6 : 0;
    std::fill_n(out + PlayfieldHeight * width, (Height - PlayfieldHeight) * width, HudBand);
    for (int y = 196; y < 228; ++y) {
        const uint32_t *source = native + y * NativeWidth;
        uint32_t *row = out + y * width;
        // Levels blank the NES's left 8 pixels to the sky colour; the panel starts after them.
        for (int x = 8; x < 168; ++x) row[margin + x - 8] = hud_color(source[x]);
        for (int x = 0; x < 88; ++x) row[width - margin - 88 + x] = hud_color(source[168 + x]);
    }
    draw_badge(out, width, margin - 8, native, ram);
}

// The world intro and game over boxes spell MARIO in background tiles. Like the HUD badge, player 1's
// character takes its place in the picture only, so the simulation stays the same for every character.
void draw_player_name(const uint8_t *ram) {
    if (!character || (ram[0x726] & 1)) return; // Player 2 keeps the ROM's LUIGI.
    static constexpr uint8_t mario[] = {0xba, 0xbc, 0xe9, 0xfc, 0xf0};
    static constexpr uint8_t names[][8] = { // The map font; $FE clears the rest of the old name.
        {}, {0xec, 0xda, 0xfc, 0xeb, 0xfc, 0xfe, 0xfe, 0xfe}, {0xea, 0xf0, 0xbc, 0xee, 0xfe, 0xfe, 0xfe, 0xfe},
        {0xea, 0xf0, 0xbc, 0xee, 0xe8, 0xea, 0xea, 0xe8}};
    std::array<int, 4 * 30> starts;
    starts.fill(-2); // -2: row not searched yet, -1: no name in it.
    for (int y = 0; y < PlayfieldHeight; ++y) {
        const int raster_y = cyc_render_line_scroll_y(y), row = raster_y % Height / 8, fine_y = raster_y % Height & 7;
        for (int x = 0; x < NativeWidth; ++x) {
            const int raster_x = (cyc_render_line_scroll_x(y) + x) & 511;
            const uint16_t table = 0x2000 | (raster_y >= Height ? 0x800 : 0) | (raster_x & 256 ? 0x400 : 0);
            int &start = starts[((table >> 10) & 3) * 30 + row];
            if (start == -2) {
                start = -1;
                for (int column = 0; column < 32 && start < 0; ++column) {
                    bool match = true;
                    for (int i = 0; i < 5; ++i)
                        match &= cyc_render_nametable(table + row * 32 + ((column + i) & 31)) == mario[i];
                    if (match) start = column;
                }
            }
            const int column = (raster_x & 255) / 8, letter = (column - start) & 31;
            if (start < 0 || letter >= 8) continue;
            const uint16_t pattern = cyc_render_line_bg_table(y) + names[character][letter] * 16 + fine_y;
            const unsigned shift = 7 - (raster_x & 7);
            // chr_byte reads the playfield's banks; the status bar switches them later in the frame.
            const unsigned value = ((chr_byte(pattern) >> shift) & 1) | (((chr_byte(pattern + 8) >> shift) & 1) << 1);
            const uint8_t attribute = cyc_render_nametable(table + 0x3c0 + row / 4 * 8 + column / 4);
            const unsigned palette = (attribute >> ((row & 2 ? 4 : 0) | (column & 2 ? 2 : 0))) & 3;
            background[y * NativeWidth + x] = cyc_render_color_value(cyc_render_palette(value ? palette * 4 + value : 0));
        }
    }
}

void draw_map(uint32_t *out, int width, int origin, const uint32_t *native, const uint8_t *ram) {
    draw_player_name(ram);
    const unsigned map_power = ram[0x3f3];
    const bool cloud_power = map_power == 7;
    std::unordered_map<uint32_t, int> counts;
    for (int y = 32; y < 184; ++y)
        for (int x = 16; x < 240; ++x) ++counts[background[y * 256 + x]];
    uint32_t ground = 0;
    int maximum = 0;
    for (const auto &entry : counts) if (entry.second > maximum) { maximum = entry.second; ground = entry.first; }
    const uint32_t ground_color = recolor(ground);
    std::vector<uint8_t> decoration(width * PlayfieldHeight, 0);
    for (int y = 0; y < PlayfieldHeight; ++y)
        for (int x = 0; x < width; ++x) {
            const int source_x = x - origin;
            uint32_t pixel = 0xffffedca;
            if (source_x >= 13 && source_x < 249 && y >= 29 && y < 192) pixel = 0xffd4b89a;
            if (source_x >= 10 && source_x < 246 && y >= 26 && y < 190) pixel = 0xffe4b18a;
            if (source_x >= 12 && source_x < 244 && y >= 28 && y < 188) pixel = 0xff292823;
            if (source_x >= 14 && source_x < 242 && y >= 30 && y < 186) pixel = ground_color;
            if (source_x >= 16 && source_x < 240 && y >= 32 && y < 184) {
                const uint32_t source = background[y * 256 + source_x];
                pixel = recolor(source);
                decoration[y * width + x] = source != ground;
            }
            out[y * width + x] = pixel;
        }
    if (shadows) smb3::shadow(out, width, PlayfieldHeight, decoration, 2, 3);
    auto &sprites = sprite_layer;
    sprites.assign(width * PlayfieldHeight, 0);
    const uint8_t *data = oam();
    const uint8_t *native_opaque = cyc_frame_bg_opaque();
    const int current = ram[0x726] & 1;
    const int expected = ram[0x77 + current] * 256 + ram[0x79 + current] - camera_x(ram);
    // The GAME OVER box shows the small player facing front: tile $61, mirrored, in slots 33 and 34.
    const bool game_over = data[33 * 4 + 1] == 0x61 && data[34 * 4 + 1] == 0x61 && data[35 * 4] >= PlayfieldHeight;
    const uint8_t *player = data + (game_over ? 33 : 35) * 4;
    const int player_x = game_over ? player[3] : smb3::unwrap(player[3], expected);
    const int pose = game_over ? 2 :
        smb3::map_pose(map_power, player[1], player[2], ram[0x8c + current] != 0, ram[0x15] & 8);
    const unsigned form = pose >= 2 ? 0 : map_power == 8 ? 3 : map_power <= 6 ? map_power : 0;
    const Image *map_sheet = character && pose >= 0 && player[0] < PlayfieldHeight ?
        &player_sheet(character, "Map.png") : nullptr;
    const bool has_map_art = map_sheet && map_sheet->width == 192 && map_sheet->height == 224;
    smb3::Sprite fallback{};
    if (map_sheet && !has_map_art && pose < 2) {
        const unsigned cell = form == 0 ? 19 + pose : form < 3 ? 18 + pose : pose;
        fallback = player_sprite(character, form, cell);
    }
    const bool replacement = has_map_art || smb3::sprite_visible(fallback);
    const int last_replaced_slot = cloud_power ? 34 : 36;
    const auto colors = character_colors(character, form);
    auto paint_map_player = [&]() {
        const int left = origin + player_x - 8, top = player[0] + 1 - 16 - (cloud_power ? 16 : 0);
        const int palette_index = 16 + (player[2] & 3) * 4;
        uint8_t palette[4];
        for (int i = 0; i < 4; ++i) palette[i] = sprite_palette(palette_index + i);
        // The warp can leave Fade_State set after the PPU has restored the map palette.
        const unsigned light = snapshot_valid && frame_cart_size >= 0x1dfe ?
            smb3::sprite_palette_light(palette, frame_cart.data() + 0x1dde + palette_index) : 255;
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x) {
                const int px = left + x, py = top + y, source_x = px - origin;
                if (px < 0 || px >= width || source_x < 0 || source_x >= NativeWidth || py < 0 || py >= PlayfieldHeight ||
                    !(cyc_render_line_mask(py) & 0x10)) continue;
                const uint32_t pixel = has_map_art ? map_sheet->at(pose * 32 + x, form * 32 + y) :
                    colors[fallback[y * 32 + x]];
                if (!(pixel >> 24)) continue;
                const unsigned role = has_map_art ? smb3::character_role(pixel, form, true) : fallback[y * 32 + x];
                const uint32_t color = ram[0x3f2] ? smb3::sprite_color(palette[role]) : smb3::dim(pixel, light);
                const int dest = py * width + px;
                sprites[dest] = smb3::sprite_priority(sprites[dest], color, player[2] & 0x20,
                    native_opaque[py * NativeWidth + source_x]);
            }
    };
    for (int slot = 63; slot >= 0; --slot) {
        if (replacement && slot >= 33 && slot <= last_replaced_slot) {
            if (slot == last_replaced_slot) paint_map_player();
            continue;
        }
        const uint8_t *sprite = data + slot * 4;
        const bool player_slot = slot >= 33 && slot <= 36;
        const int column = player_slot ? smb3::unwrap(sprite[3], player_x) : sprite[3];
        const int top = sprite[0] + 1, left = origin + column;
        for (int y = 0; y < 16 && top + y < PlayfieldHeight; ++y) {
            const auto row = sprite_row(sprite, y);
            for (int x = 0; x < 8 && left + x < width; ++x) {
                if (left + x < 0 || column + x < 0 || column + x >= 256) continue;
                const unsigned value = row.pixel(x);
                if (!value) continue;
                const unsigned color = sprite_palette(16 + (sprite[2] & 3) * 4 + value);
                const uint32_t pixel = smb3::sprite_color(player_slot && !cloud_power && !ram[0x3f2] &&
                    (color == 0x16 || color == 0x2a) ?
                    (character ? 0x2a : 0x16) : color);
                const int dest = (top + y) * width + left + x;
                sprites[dest] = smb3::sprite_priority(sprites[dest], pixel, sprite[2] & 0x20,
                    native_opaque[(top + y) * 256 + column + x]);
            }
        }
    }
    if (shadows) smb3::shadow(out, width, PlayfieldHeight, sprites);
    for (size_t i = 0; i < sprites.size(); ++i) if (sprites[i]) out[i] = sprites[i];
    draw_hud(out, width, origin, native, inventory_visible(ram), ram);
}

struct TitleSpritePatch {
    static constexpr int Size = 32;
    static constexpr uint32_t RevealBackground = 1;
    int left = 0, top = 0;
    std::array<uint32_t, Size * Size> pixels{};

    uint32_t at(int x, int y) const {
        x -= left;
        y -= top;
        return x >= 0 && x < Size && y >= 0 && y < Size ? pixels[y * Size + x] : 0;
    }
};

TitleSpritePatch title_luigi(const uint8_t *ram) {
    TitleSpritePatch patch;
    if (ram[0xde] > 1 || ram[0xc2] != 1 || ram[0xc4] != 0x28) return patch;
    constexpr int first = 0x28 / 4, last = first + 5, size = TitleSpritePatch::Size;
    const auto *data = oam();
    const auto *opaque = cyc_frame_bg_opaque();
    patch.left = static_cast<int8_t>(ram[0x76]) * 256 + ram[0x86] - 8;
    patch.top = static_cast<int8_t>(ram[0x7e]) * 256 + ram[0x8e] + 1;
    const int left = std::max(0, patch.left), right = std::min(NativeWidth, patch.left + size);
    const int top = std::max(0, patch.top), bottom = std::min(PlayfieldHeight, patch.top + size);
    if (left >= right || top >= bottom) return patch;

    auto visit_sprite = [&](int slot, const auto &paint) {
        const auto *sprite = data + 4 * slot;
        const int x0 = sprite[3], y0 = sprite[0] + 1;
        for (int y = std::max(top, y0); y < std::min(bottom, y0 + 16); ++y) {
            const uint8_t mask = cyc_render_line_mask(y);
            if (!(mask & 0x10)) continue;
            const int clip = mask & 4 ? left : std::max(left, 8);
            const auto row = sprite_row(sprite, y - y0);
            for (int x = std::max(clip, x0); x < std::min(right, x0 + 8); ++x) {
                const unsigned role = row.pixel(x - x0);
                if (role) paint(x, y, role, sprite[2]);
            }
        }
    };
    auto index = [&](int x, int y) { return (y - patch.top) * size + x - patch.left; };
    smb3::Sprite original{};
    for (int slot = last; slot >= first; --slot) {
        visit_sprite(slot, [&](int x, int y, unsigned role, uint8_t) { original[index(x, y)] = role; });
    }
    if (!smb3::sprite_visible(original)) return patch;

    const bool falling = ram[0xb6] && !(ram[0x9e] & 0x80);
    auto sprite = smb3::campaign_sprite(player_sheet(1, "Campaign.png"), 1, smb3::title_luigi_frame(ram[0xc0], falling));
    const uint8_t flags = ram[0xb8];
    if (!smb3::sprite_visible(sprite)) sprite = smb3::luigi_silhouette(original);
    else if (!(flags & 0x40)) {
        for (int y = 0; y < size; ++y) std::reverse(sprite.begin() + y * size, sprite.begin() + (y + 1) * size);
    }

    // Repaint only the pixels covered by one of Luigi's silhouettes, in OAM order.
    for (int slot = 63; slot >= 0; --slot) {
        if (slot >= first && slot <= last) {
            if (slot != last) continue;
            for (int y = top; y < bottom; ++y)
                for (int x = left; x < right; ++x) {
                    const int i = index(x, y);
                    const uint8_t mask = cyc_render_line_mask(y);
                    if (!sprite[i] || !(mask & 0x10) || (x < 8 && !(mask & 4))) continue;
                    patch.pixels[i] = smb3::sprite_color(sprite_palette(20 + sprite[i]));
                }
            continue;
        }
        visit_sprite(slot, [&](int x, int y, unsigned role, uint8_t attributes) {
            const int i = index(x, y);
            if (!original[i] && !sprite[i]) return;
            const auto color = smb3::sprite_color(sprite_palette(16 + (attributes & 3) * 4 + role));
            patch.pixels[i] = smb3::sprite_priority(patch.pixels[i], color, attributes & 0x20, opaque[y * NativeWidth + x]);
        });
    }
    for (size_t i = 0; i < patch.pixels.size(); ++i)
        if (original[i] && !patch.pixels[i]) patch.pixels[i] = TitleSpritePatch::RevealBackground;
    return patch;
}

bool in_title(const uint8_t *ram) {
    // The credits share this raster mode and use a different background CHR bank.
    return ram[0x101] == 0x20 && ram[0x719] == 0x78;
}

void draw_title(uint32_t *out, int width, int origin, const uint32_t *native, const uint8_t *ram) {
    const uint32_t stage = cyc_render_color_value(sprite_palette(0));
    const uint32_t backdrop = smb3::color(sprite_palette(0));
    const auto luigi = title_luigi(ram);
    // The native intro skip also loads the old menu. Its three text rows share the same tile span.
    const bool native_menu = frontend.active && ram[0xde] >= 2;
    const auto *cursor = oam() + 60 * 4;
    auto actor_pixel = [&](int column, int y) {
        if (column < 0 || column >= NativeWidth) return uint32_t{0};
        if (native_menu && cursor[1] == 0xdf && cursor[3] == 72 && (cursor[0] == 151 || cursor[0] == 167) &&
            column >= 72 && column < 80 && y > cursor[0] && y <= cursor[0] + 16) return uint32_t{0};
        const uint32_t replacement = luigi.at(column, y);
        if (replacement) return replacement == TitleSpritePatch::RevealBackground ? 0 : replacement;
        const int i = y * NativeWidth + column;
        return native[i] != background[i] ? recolor(native[i], true) : 0;
    };
    const int curtain_height = ram[0xde] == 0 ? PlayfieldHeight : 24;
    const bool curtain_visible = sprite_palette(3) != sprite_palette(0);
    for (int y = 0; y < Height; ++y) {
        const auto *row = background.data() + y * NativeWidth;
        const bool floor = y >= PlayfieldHeight;
        const bool fabric = curtain_visible && !floor && cyc_render_line_scroll_y(y) < curtain_height &&
                            smb3::stage_repeating_row(row);
        const bool restore_left = native_menu && !(cyc_render_line_mask(y) & 2);
        const int tile_row = y / 8;
        const uint8_t *left_tile = nullptr;
        int left_palette = 0;
        if (restore_left && !floor && !fabric) {
            const auto pattern = cyc_render_nametable(0x2000 + tile_row * 32);
            left_tile = rom.data() + ChrStart + 0x78 * 1024 + pattern * 16;
            const auto attribute = cyc_render_nametable(0x23c0 + tile_row / 4 * 8);
            left_palette = ((attribute >> (tile_row & 2 ? 4 : 0)) & 3) * 4;
        }
        for (int x = 0; x < width; ++x) {
            const int column = x - origin;
            const bool inside = column >= 0 && column < NativeWidth;
            const bool menu_text = native_menu && column >= 80 && column < 184 &&
                (y / 8 == 19 || y / 8 == 21 || y / 8 == 23);
            const int sample = smb3::StagePatternPeriod + smb3::wrap(column, smb3::StagePatternPeriod);
            uint32_t art = menu_text ? stage : inside ? row[column] : (floor || fabric ? row[sample] : stage);
            if (restore_left && column >= 0 && column < 8) {
                // Skipping the intro enables the NES left-column clip, which would leave a seam in widescreen.
                if (left_tile) {
                    const int color = smb3::tile_pixel(left_tile, column, y & 7);
                    art = color ? cyc_render_color_value(sprite_palette(left_palette + color)) : stage;
                } else art = row[sample];
            }
            uint32_t pixel = recolor(art);
            if (floor) {
                if (pixel == smb3::color(0x0f)) pixel = smb3::StageInk;
                else if (pixel == smb3::color(0x30) || pixel == smb3::color(0x20)) pixel = smb3::StageIvory;
            } else if (fabric) {
                pixel = smb3::stage_fabric(pixel, column);
            } else if (art == stage) {
                pixel = smb3::stage_backdrop(backdrop, x, y, width, PlayfieldHeight);
            }
            const uint32_t actor = actor_pixel(column, y);
            if (actor) pixel = actor;
            else if (shadows && !floor && !fabric && art == stage && y >= 4 && actor_pixel(column - 3, y - 4))
                pixel = smb3::blend(pixel, 0x55000000);
            out[y * width + x] = pixel;
        }
    }
}

int compose_game(uint32_t *out, int width, int height, int native_x0, const uint32_t *native, void *) {
    if (height != Height) return 0;
    const uint8_t *ram = snapshot_valid ? frame_ram.data() : cyc_cpu_ram();
    const bool title = in_title(ram);
    const bool starting = cyc_cycle_count() < 300000 || (frontend.active && frontend.screen == smb3::Screen::Intro);
    if ((title || starting) && !(cyc_render_line_mask(80) & 0x18)) {
        std::fill_n(out, width * Height, smb3::StageInk);
        return 1;
    }
    if (!cyc_render_background(background.data())) return 0;
    frame_colors.clear();
    for (unsigned color = 0; color < 64; ++color) frame_colors.emplace(cyc_render_color_value(color), color);
    const int tileset = ram[0x70a], world = std::min<int>(ram[0x727], 7);
    bool black = true;
    for (int i = 0; i < 32; ++i)
        if ((cyc_render_palette(i) & 15) < 13 && cyc_render_palette(i) != 0x0f) black = false;
    if (black) return 0;
    if (title) {
        cyc_render_set_native_origin(native_x0);
        draw_title(out, width, native_x0, native, ram);
        return 1;
    }
    static std::array<uint32_t, 64> console_colors;
    for (unsigned color = 0; color < 64; ++color) console_colors[color] = cyc_render_color_value(color);
    if (!enhanced) smb3::classic_palette = console_colors.data(); // compose() restores the remaster's.
    if (ram[0x101] == 0x20) {
        cyc_render_set_native_origin(native_x0);
        for (int y = 0; y < Height; ++y) {
            std::unordered_map<uint32_t, int> counts;
            uint32_t stage = background[y * 256];
            int maximum = 0;
            for (int x = 0; x < 256; ++x) {
                const uint32_t pixel = background[y * 256 + x];
                if (++counts[pixel] > maximum) { maximum = counts[pixel]; stage = pixel; }
            }
            for (int x = 0; x < width; ++x) {
                const int column = x - native_x0;
                const int repeat = y >= 192 ? 32 : 16;
                const int sample = smb3::wrap(column, repeat);
                const int source = y * 256 + std::clamp(column, 0, 255);
                if (column >= 0 && column < 256)
                    out[y * width + x] = recolor(native[source], native[source] != background[source]);
                else out[y * width + x] = recolor(y >= 192 || y < 24 ? background[y * 256 + sample] : stage);
            }
        }
        return 1;
    }
    const bool level = tileset > 0 && tileset < 15 && (cyc_render_line_mask(80) & 8);
    const bool map = tileset == 0 && ram[0x101] == 0 && ram[0x729] < 0x11;
    if (map) { draw_map(out, width, native_x0, native, ram); return 1; }
    const bool toad_house = tileset == 7;
    size_t cart_size = frame_cart_size;
    const uint8_t *cart = snapshot_valid ? frame_cart.data() : cyc_cart_ram(&cart_size);
    const bool horizontal = level && !ram[0x3ef];
    const int end = std::min<int>(ram[0x22] + 1, 15) * NativeWidth;
    const bool rebuild = horizontal && cart && cart_size >= size_t(end / NativeWidth * LevelScreenTiles) &&
                         rom.size() == 393216;
    const int camera = horizontal ? camera_x(ram) : 0;
    const int goal_x = horizontal ? smb3::goal_position(cart, cart_size) : -1;
    const int edge = rebuild && goal_x >= 0 && goal_x < end ? goal_edge(cart, goal_x) : -1;
    const bool course_clear = level && cart && cart_size > 0x1974 && (cart[0x1974] & 0x80);
    const bool result_text = course_clear && ram[0x71a] == ResultChrBank;
    const bool bonus_scene = course_clear && ram[0x719] == ResultChrBank;
    const int origin = horizontal ? smb3::level_origin(camera, width, end, goal_x) : native_x0;
    cyc_render_set_native_origin(origin);
    const int layout = cart && cart_size > 0x1ebd ? cart[0x1ebd] : 0;
    // Pal_Data keeps the room's original backdrop color through fades and palette effects.
    const unsigned background_color = cart && cart_size > 0x1dde ? cart[0x1dde] : sprite_palette(0);
    const smb3::Backdrop style = smb3::level_backdrop(world, tileset, layout, ram[0x462], background_color, ram[0x73a]);
    const int tick = static_cast<int>((cyc_cycle_count() / 29781) & 0x3fffffff);
    const unsigned light = level ? scenery_light(ram) : 255;
    if (bonus_scene) std::fill_n(out, width * PlayfieldHeight, GoalBlack);
    else {
        if (enhanced) backdrop(out, width, camera - origin, horizontal ? ram[0x543] : 239, style, tick);
        else {
            std::fill_n(out, width * PlayfieldHeight, smb3::color(background_color));
            ambient_particles(out, width, camera - origin, tick, style.atmosphere);
        }
        if (light < 255)
            for (int i = 0; i < width * PlayfieldHeight; ++i) out[i] = smb3::dim(out[i], light);
    }
    const int rows = level ? PlayfieldHeight : Height;
    static std::vector<uint8_t> opaque;
    static std::vector<uint32_t> terrain, water;
    opaque.assign(width * rows, 0);
    terrain.assign(width * rows, 0);
    water.clear();
    const uint8_t *native_opaque = cyc_frame_bg_opaque();
    static std::vector<uint8_t> written;
    written.assign(NativeWidth * PlayfieldHeight, 0);
    if (rebuild && !bonus_scene) {
        uint8_t chr_banks[] = {ram[0x719], ram[0x71a]};
        // The result and dialogue font takes over the second bank. Keep the room's terrain graphics.
        if (ram[0x71a] == ResultChrBank && layout < 23)
            chr_banks[1] = rom[0x3d779 + layout];
        static const Image classic_tile;
        auto pack = [&](const char *name) -> const Image & { return enhanced ? image(name) : classic_tile; };
        const TileFrames frames{pack("Blocks/QuestionBlock.png"), pack("Tilesets/Terrain/BrickBlock.png"),
            pack("Items/StaticCoin.png"), (tick / 12 % 4) * 16,
            smb3::atlas_row(style.scenery, false) * 16, smb3::atlas_row(style.scenery, true) * 16, light};
        // Level_MinTileUWByQuad and Tile_AttrTable give the swimming range of each palette group.
        const uint8_t *water_limits = rom.data() + 0x200 + (tileset - 1) * 4;
        const uint8_t *solid_limits = cart_size > 0x1e97 ? cart + 0x1e94 : nullptr;
        const uint32_t water_body = smb3::dim(style.atmosphere == smb3::Atmosphere::Bubbles ? 0x32477fac : 0x884386ca, light);
        const uint32_t water_crest = smb3::dim(0xbcdcf6ff, light);
        // Decode each visible metatile once with this frame's CHR bank, palette and animation.
        static std::array<MetatileFrame, 512> metatiles;
        std::array<bool, 512> decoded{};
        auto metatile = [&](unsigned tile, bool goal) -> const MetatileFrame & {
            const unsigned key = tile + (goal ? 256 : 0);
            auto &image = metatiles[key];
            if (decoded[key]) return image;
            const auto material = solid_limits ? smb3::water_tile(tileset, tile, water_limits, solid_limits) :
                smb3::WaterTile::Dry;
            const bool liquid = material == smb3::WaterTile::Fill || material == smb3::WaterTile::Wave;
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 16; ++x) {
                    const int i = y * 16 + x;
                    const unsigned texel = terrain_texel(tile, tileset, chr_banks, x, y);
                    image.water[i] = smb3::water_overlay(material, texel, y, water_body, water_crest);
                    image.pixels[i] = liquid ? 0 : terrain_pixel(tile, texel, tileset, x, y, goal, frames);
                }
            decoded[key] = true;
            return image;
        };
        int water_line = PlayfieldHeight;
        int ordinary_y = 0;
        if (smb3::water_partition(ram[0x100], ram[0x101], ram[0x58b] != 0)) {
            constexpr int Split = PlayfieldHeight - 32;
            constexpr int LastTileY = LevelRows * 16;
            ordinary_y = cyc_render_line_scroll_y(Split - 1) - (Split - 1);
            for (int y = Split; y < PlayfieldHeight; ++y) {
                const int raster_y = cyc_render_line_scroll_y(y);
                if (raster_y >= LastTileY - 32 && raster_y < LastTileY && raster_y != ordinary_y + y) {
                    water_line = y;
                    break;
                }
            }
        }
        for (int y = 0; y < PlayfieldHeight; ++y) {
            const int raster_y = cyc_render_line_scroll_y(y);
            const bool fixed_water = y >= water_line;
            // The IRQ displays pond tiles here. Continue the room behind that water strip.
            const int wy = fixed_water ? ordinary_y + y : raster_y;
            const bool room_row = wy >= 0 && wy < LevelRows * 16;
            if (!fixed_water && !room_row) continue;
            const bool wave_row = fixed_water && y - water_line < 8 && raster_y < LevelRows * 16;
            // Extend the interior floor at the goal without repeating its terminal edge.
            const int repeat_from = edge >= 0 && wy / 16 == LevelRows - 1 ? end - 32 : end - 16;
            const int repeat_after = repeat_from + 16;
            for (int x = std::max(0, origin - camera); x < width;) {
                int wx = camera + x - origin;
                if (goal_x >= 0 && wx >= repeat_after) wx = repeat_from + (wx & 15);
                if (wx >= end) break;
                const int count = std::min(width - x, 16 - (wx & 15));
                const int texel = (wy & 15) * 16 + (wx & 15);
                const auto *tile = room_row ? &metatile(cart[level_tile_address(wx, wy)], edge >= 0 && wx >= edge) : nullptr;
                const unsigned wave = wave_row ? cart[level_tile_address(wx, raster_y)] : 0;
                for (int j = 0; j < count; ++j) {
                    const int dest = y * width + x + j;
                    uint32_t liquid = fixed_water ? water_body : 0;
                    if (tile) {
                        const uint32_t pixel = tile->pixels[texel + j];
                        terrain[dest] = pixel;
                        opaque[dest] = pixel >> 24 != 0;
                        if (!fixed_water) liquid = tile->water[texel + j];
                    }
                    if (wave_row) {
                        const unsigned value = terrain_texel(wave, tileset, chr_banks, wx + j, raster_y);
                        liquid = smb3::water_overlay(smb3::WaterTile::Wave, value, raster_y & 15, water_body, water_crest);
                    }
                    if (liquid) {
                        if (water.empty()) water.resize(width * PlayfieldHeight);
                        water[dest] = liquid;
                    }
                }
                x += count;
            }
        }
        // The king's dialogue, the princess's letter and other scenes write text straight to the
        // nametable. Keep the PPU's picture wherever it no longer matches the room data. The NES
        // edge columns are skipped: they hold the column being streamed in.
        for (int y = 0; y < water_line; ++y) {
            const int raster_y = cyc_render_line_scroll_y(y), scroll = cyc_render_line_scroll_x(y);
            if (raster_y >= LevelRows * 16 || ((scroll - camera) & 255)) continue; // Rows a raster split moved.
            const uint16_t row = (raster_y >= Height ? 0x2800 : 0x2000) + raster_y % Height / 8 * 32;
            bool changed = false;
            for (int x = 16; x < NativeWidth - 16; ++x) {
                const int wx = camera + x, ppu_x = (scroll + x) & 511;
                if (wx >= end - 16) break;
                if (x == 16 || !(wx & 7))
                    changed = cyc_render_nametable((ppu_x & 256 ? row | 0x400 : row) + (ppu_x & 255) / 8) !=
                        terrain_pattern(cart[level_tile_address(wx, raster_y)], tileset, wx, raster_y);
                if (changed) written[y * NativeWidth + x] = 1;
            }
        }
    }
    const uint32_t native_black = cyc_render_color_value(0x0f);
    const bool overlays_only = rebuild && !bonus_scene;
    for (int y = 0; y < rows; ++y) {
        const int wy = cyc_render_line_scroll_y(y);
        // The normal result writes the two lines and the card into rows 4 through 9 of $2800.
        const bool result_row = result_text && wy >= Height + 4 * 8 && wy < Height + 10 * 8;
        // Toad types into $28C8. Keep the live 17-by-8-tile box over the remastered room.
        const bool dialogue_row = overlays_only && toad_house && wy >= Height + 6 * 8 && wy < Height + 14 * 8;
        const bool written_row = overlays_only && !result_row && y < PlayfieldHeight &&
            std::any_of(written.begin() + y * NativeWidth, written.begin() + (y + 1) * NativeWidth, [](uint8_t w) { return w; });
        if (overlays_only && !dialogue_row && !result_row && !written_row) continue;
        for (int x = 0; x < NativeWidth; ++x) {
            const int source = y * NativeWidth + x;
            // Text boxes cover the room, backdrop pixels included.
            const bool box = (dialogue_row && x >= 8 * 8 && x < 25 * 8) || (written_row && written[source]);
            if (overlays_only && !box && !(result_row && !dialogue_row && background[source] != native_black)) continue;
            const int dest = y * width + origin + x;
            if (box || !level || native_opaque[source]) {
                opaque[dest] = box ? native_opaque[source] : 1;
                terrain[dest] = recolor(level ? background[source] : native[source]);
                if (bonus_scene && y < PlayfieldHeight && background[source] == native_black)
                    terrain[dest] = GoalBlack;
            }
        }
    }
    if (shadows && level) smb3::shadow(out, width, PlayfieldHeight, opaque);
    for (size_t i = 0; i < terrain.size(); ++i) {
        if (terrain[i] >> 24 == 255) out[i] = terrain[i];
        else if (terrain[i] >> 24) out[i] = smb3::blend(out[i], terrain[i]);
    }
    if (level) draw_sprites(out, width, origin, ram, opaque);
    for (size_t i = 0; i < water.size(); ++i)
        if (water[i]) out[i] = smb3::blend(out[i], water[i]);
    if (level) draw_hud(out, width, native_x0, native, toad_house && inventory_visible(ram), ram);
    return 1;
}

int compose(uint32_t *out, int width, int height, int native_x0, const uint32_t *native, void *context) {
#ifdef CYC_WITH_RECOMP_UI
    const bool on_title = frontend.shows_title() && !title_picture.empty();
    const Image &picture = on_title ? title_picture : paused_picture;
    if (!picture.empty() && height == picture.height) {
        const int origin = on_title ? title_native_origin : (picture.width - NativeWidth) / 2;
        if (width == picture.width && native_x0 == origin) {
            std::copy(picture.pixels.begin(), picture.pixels.end(), out);
        } else {
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x) {
                    int column = x - native_x0 + origin;
                    if (on_title && (column < 0 || column >= picture.width) && (y < 24 || y >= PlayfieldHeight))
                        column = smb3::wrap(column, smb3::StagePatternPeriod);
                    out[y * width + x] = picture.at(std::clamp(column, 0, picture.width - 1), y);
                }
        }
        cyc_render_set_native_origin(native_x0);
        return 1;
    }
#endif
    const int composed = compose_game(out, width, height, native_x0, native, context);
    smb3::classic_palette = nullptr; // The menus always use the remaster's colors.
    return composed;
}

#ifdef CYC_WITH_RECOMP_UI
Image current_picture() {
    int width, height;
    const auto *pixels = cyc_render_present(&width, &height);
    return {width, height, std::vector<uint32_t>(pixels, pixels + width * height)};
}

uint8_t pause_vblank_read(uint16_t pc, uint16_t address, uint8_t value) {
    return pc == paused_wait && address == VBlankTick ? 0 : value;
}

void hold_pause() {
    paused_picture = current_picture();
    CycCpuState cpu{};
    cyc_cpu_state(&cpu);
    // Hold the main thread at its ordinary VBlank wait. NMI still calls the ROM sound driver.
    for (uint16_t wait : {CommonWait, TitleWait}) {
        if ((cpu.pc == wait || cpu.pc == wait + 2) && cyc_mod_peek(wait) == 0xa5 &&
            cyc_mod_peek(wait + 1) == VBlankTick && cyc_mod_peek(wait + 2) == 0x10 &&
            cyc_mod_peek(wait + 3) == 0xfc) {
            paused_wait = wait;
            cyc_mod_set_ram_read_hook(pause_vblank_read);
            break;
        }
    }
    cyc_render_frame_done();
}

void release_pause() {
    if (paused_wait) cyc_mod_set_ram_read_hook(nullptr);
    paused_wait = 0;
    paused_picture = {};
    cyc_render_frame_done();
}

void filter_pause_audio(void *, int16_t *samples, size_t count) {
    static float filtered = 0, wet = 0;
    const float target = paused_wait ? 1.f : 0.f;
    for (size_t i = 0; i < count; ++i) {
        filtered += .10f * (samples[i] - filtered);
        wet += .003f * (target - wet);
        samples[i] = static_cast<int16_t>(samples[i] * (1.f - wet) + filtered * .65f * wet);
    }
}
#endif

void power_on(void *) {
    snapshot_valid = false;
#ifdef CYC_WITH_RECOMP_UI
    release_pause();
    title_picture = {};
    paused_buttons = {};
#endif
#ifdef SMB3_RUNTIME_ROM
    cyc_run_native = false;
#endif
    set_view(nullptr, view);
    nes_mod_set_function_hook_enabled("smb3.wide-visibility", 1);
    nes_mod_set_function_hook_enabled("smb3.wide-spawn", 1);
    nes_mod_set_function_hook_enabled("smb3.wide-projectile-origin", 1);
    nes_mod_set_function_hook_enabled("smb3.wide-projectile", 1);
    nes_mod_set_function_hook_enabled("smb3.wide-projectile-hit", 1);
    nes_mod_set_function_hook_enabled("smb3.wide-player-projectile", 1);
    nes_mod_set_function_hook_enabled("smb3.wide-player-projectile-hit", 1);
    nes_mod_set_function_hook_enabled("smb3.wide-cannon", 1);
    nes_mod_set_function_hook_enabled("smb3.wide-actor-draw-start", 1);
    nes_mod_set_function_hook_enabled("smb3.wide-actor-draw-end", 1);
    nes_mod_set_function_hook_enabled("smb3.wide-actor-retain", 1);
    nes_mod_set_function_hook_enabled("smb3.wide-box-origin", 1);
    nes_mod_set_function_hook_enabled("smb3.wide-box-intersect", 1);
    nes_mod_set_function_hook_enabled("smb3.wide-shell-blocks", 1);
    nes_mod_set_function_hook_enabled("smb3.wide-player-crush", 1);
    nes_mod_set_function_hook_enabled("smb3.wide-player-boundary", 1);
    nes_mod_set_function_hook_enabled("smb3.wide-autoscroll", 1);
    for (const char *id : {"smb3.coop-player", "smb3.coop-player-done", "smb3.coop-scrolling", "smb3.coop-die",
                           "smb3.coop-throw", "smb3.coop-special-done", "smb3.coop-block-flower", "smb3.coop-block-leaf",
                           "smb3.coop-take-flower", "smb3.coop-take-leaf"})
        nes_mod_set_function_hook_enabled(id, 1);
    smb3::extra_sprites = {};
    smb3::displayed_extra_sprites = {};
    smb3::drawing_actor = -1;
    cyc_render_set_compositor(compose, nullptr);
    cyc_render_set_stock_compositing(true);
    cyc_render_capture_background(true);
    rom.resize(cyc_mod_copy_cart_payload(nullptr, 0));
    if (!rom.empty()) cyc_mod_copy_cart_payload(rom.data(), rom.size());
    smb3::coop_palettes = rom.size() == 393216 ? rom.data() + 0x10529 : nullptr; // PowerUp_Palettes
    smb3::coop.pass = smb3::CoopPass::Idle;
    smb3::coop.swapped = 0;
#ifdef CYC_WITH_RECOMP_UI
    nes_mod_set_function_hook_enabled("smb3.practice-world", 1);
    nes_mod_set_function_hook_enabled("smb3.practice-level", 1);
    practice_target = nullptr;
    practice_levels = smb3::practice_levels(rom);
    for (const auto &levels : practice_levels)
        for (const auto &level : levels)
            if (requested_level == (level.number ? level.label(false) : std::to_string(level.world + 1) + "-" + level.label(false)))
                practice_target = &level;
    if (!requested_level.empty() && !practice_target)
        std::fprintf(stderr, "SMB3 Remastered: no hay un nivel %s\n", requested_level.c_str());
    practice_campaign.data.reset();
    practice_boot.data.reset();
    practice_boot.capture();
#endif
    original_player.fill(0);
    if (rom.size() == 393216) {
        const int patterns[] = {3, 1, 7, 5};
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 16; ++x) {
                const int pattern = patterns[(y / 16) * 2 + x / 8];
                const uint8_t *tile = rom.data() + ChrStart + 84 * 1024 + (pattern & 0xfe) * 16 + (y % 16 / 8) * 16;
                original_player[y * 32 + x + 8] = smb3::tile_pixel(tile, 7 - (x & 7), y & 7);
            }
    }
    load_local_art();
#ifdef CYC_WITH_RECOMP_UI
    read_campaign_save_info();
#endif
    if (character >= character_count) character = 0;
    const bool local_art = !pack_sprites.empty();
    std::fprintf(stderr, "SMB3 Remastered: %s; personaje %s; arte local %s\n",
                 enhanced ? "activado" : "clasico", Characters[character],
                 local_art ? "disponible" : "no instalado (ROM)");
}

#ifdef CYC_WITH_RECOMP_UI
bool practice_in_level() {
    const auto *ram = cyc_cpu_ram();
    return frontend.started && ram[0x101] != 0x20 && ram[0x70a] > 0 && ram[0x70a] < 15 &&
        ram[0xeb] && !ram[0xf1] && !ram[0x14] && !ram[0x559] && !ram[0x571];
}

bool practice_on_map() {
    const auto *ram = cyc_cpu_ram();
    return frontend.started && ram[0x101] != 0x20 && !ram[0x70a] && ram[0x729] == 0x0d;
}

bool practice_checkpoint() {
    if (practice_campaign.data) return true;
    if (!practice_campaign.capture()) {
        practice_error = smb3_text("Could not save the campaign checkpoint.", "No se pudo guardar el punto de regreso.");
        return false;
    }
    campaign_frontend = frontend;
    campaign_frontend.screen = frontend.settings_return == smb3::Screen::Pause
        ? smb3::Screen::Playing : frontend.settings_return;
    return true;
}

void practice_resume() {
    release_pause();
    recomp_runtime_ui_close(cyc_ui_runtime());
    frontend_screen(smb3::Screen::Playing);
}

void practice_apply_power(int power) {
    if (!practice_checkpoint()) return;
    if (power == PracticeStar) {
        cyc_mod_poke(0x553, 0xe0); // Player_StarInv
        cyc_mod_poke(0x4f5, 0x0a); // Sound_QMusic2: invincibility
    } else {
        const bool wing = power == PracticeWing;
        const int suit = wing ? 3 : power;
        cyc_mod_poke(0x578, suit + 1); // Player_QueueSuit
        if (suit != 5) cyc_mod_poke(0x57a, 0); // Player_Statue
        if (wing || cyc_cpu_ram()[0x56e] == 0xff) {
            cyc_mod_poke(0x3dd, wing ? 0x7f : 0); // Player_Power
            cyc_mod_poke(0x56e, wing ? 0xff : 0); // Player_FlyTime
        }
    }
    practice_resume();
}

void practice_give_lives() {
    if (!practice_checkpoint()) return;
    cyc_mod_poke(0x736 + (cyc_cpu_ram()[0x726] & 1), 99);
    practice_resume();
}

void practice_warp(const smb3::PracticeLevel &level, int power) {
    const bool first_test = !practice_campaign.data;
    if (!practice_checkpoint()) return;
    const auto *ram = cyc_cpu_ram();
    const int player = ram[0x726] & 1;
    const bool in_level = practice_in_level(), on_map = practice_on_map();
    const int suit = in_level ? ram[0xed] : on_map ? ram[0x746 + player] : 0;
    const int lives = in_level || on_map ? ram[0x736 + player] : -1;
    if (!practice_boot.restore()) {
        if (first_test) practice_campaign.data.reset();
        practice_error = smb3_text("Could not start the practice level.", "No se pudo iniciar el nivel de prueba.");
        return;
    }
    cyc_state_ui_reset();
    practice_target = &level;
    practice_warp_power = power;
    practice_warp_suit = power == PracticeWing ? 3 : power == PracticeStar ? std::min(suit, 6) : power;
    practice_warp_lives = lives >= 0 && lives <= 99 ? lives : -1;
    frontend.started = false;
    frontend.players = 1;
    frontend.play();
    practice_resume();
}

void practice_restore_campaign() {
    if (!practice_campaign.restore()) {
        practice_error = smb3_text("Could not restore the campaign checkpoint.", "No se pudo recuperar el punto de regreso.");
        return;
    }
    practice_campaign.data.reset();
    cyc_state_ui_reset();
    recomp_runtime_ui_close(cyc_ui_runtime());
    frontend = campaign_frontend;
    frontend_screen(campaign_frontend.screen);
}

const char *const Styles[] = {"Classic NES", "Remastered"};
const char *const HudStyles[] = {"Classic", "Remastered"};
const char *const StarStyles[] = {"Classic", "Rainbow"};
const char *const Languages[] = {"English", "Español"};
const RecompRuntimeUiItem MenuItems[] = {
    {"smb3.language", "System", "Language", "Choose the language of the menus.",
     RECOMP_RUNTIME_UI_CHOICE, 0, 1, 1, Languages, 2, nullptr},
    {"smb3.home", "Remastered", "Main menu", "Return to the title screen without ending the session.",
     RECOMP_RUNTIME_UI_ACTION, 0, 0, 0, nullptr, 0, nullptr},
    {"smb3.character", "Remastered", "Characters", "Open character selection.",
     RECOMP_RUNTIME_UI_ACTION, 0, 0, 0, nullptr, 0, nullptr},
    {"smb3.graphics", "Remastered", "Visual style", "Classic NES art keeps the widescreen view and every option below.",
     RECOMP_RUNTIME_UI_CHOICE, 0, 1, 1, Styles, 2, nullptr},
    {"smb3.shadows", "Remastered", "Shadows", "Show scenery and character shadows.",
     RECOMP_RUNTIME_UI_BOOL, 0, 1, 1, nullptr, 0, nullptr},
    {"smb3.particles", "Remastered", "Background particles", "Snow, embers, bubbles and wind over the scenery.",
     RECOMP_RUNTIME_UI_BOOL, 0, 1, 1, nullptr, 0, nullptr},
    {"smb3.parallax", "Remastered", "Parallax", "Far scenery scrolls slower than the ground.",
     RECOMP_RUNTIME_UI_BOOL, 0, 1, 1, nullptr, 0, nullptr},
    {"smb3.hud", "Remastered", "HUD", "Split panels at the screen edges, or the NES status bar.",
     RECOMP_RUNTIME_UI_CHOICE, 0, 1, 1, HudStyles, 2, nullptr},
    {"smb3.star", "Remastered", "Star effect", "Colors of the player during star power.",
     RECOMP_RUNTIME_UI_CHOICE, 0, 1, 1, StarStyles, 2, nullptr},
    {"smb3.practice", "Practice", "Tools", "Test powers, lives, and levels.",
     RECOMP_RUNTIME_UI_ACTION, 0, 0, 0, nullptr, 0, nullptr},
};
int menu_get(void *, const RecompRuntimeUiItem *item, int *value) {
    if (!std::strcmp(item->key, "smb3.language")) *value = spanish_ui;
    else if (!std::strcmp(item->key, "smb3.graphics")) *value = enhanced;
    else if (!std::strcmp(item->key, "smb3.shadows")) *value = shadows;
    else if (!std::strcmp(item->key, "smb3.particles")) *value = particles;
    else if (!std::strcmp(item->key, "smb3.parallax")) *value = parallax;
    else if (!std::strcmp(item->key, "smb3.hud")) *value = modern_hud;
    else if (!std::strcmp(item->key, "smb3.star")) *value = rainbow_star;
    else return 0;
    return 1;
}
int menu_set(void *, const RecompRuntimeUiItem *item, int value) {
    if (value < 0 || value > 1) return 0;
    if (!std::strcmp(item->key, "smb3.language")) {
        spanish_ui = value == 1;
        return 1;
    }
    if (!std::strcmp(item->key, "smb3.graphics")) enhanced = value;
    else if (!std::strcmp(item->key, "smb3.shadows")) shadows = value;
    else if (!std::strcmp(item->key, "smb3.particles")) particles = value;
    else if (!std::strcmp(item->key, "smb3.parallax")) parallax = value;
    else if (!std::strcmp(item->key, "smb3.hud")) modern_hud = value;
    else if (!std::strcmp(item->key, "smb3.star")) rainbow_star = value;
    else return 0;
    cyc_render_frame_done();
    return 1;
}
int menu_action(void *, const RecompRuntimeUiItem *item) {
    if (!std::strcmp(item->key, "smb3.character")) {
        frontend_characters(smb3::Screen::Settings, false);
        return 1;
    }
    if (!std::strcmp(item->key, "smb3.practice")) {
        frontend_screen(smb3::Screen::Practice);
        return 1;
    }
    if (std::strcmp(item->key, "smb3.home")) return 0;
    release_pause();
    frontend_screen(smb3::Screen::Main);
    return 1;
}
const RecompRuntimeUiCallbacks MenuCallbacks = {nullptr, menu_get, menu_set, menu_action};

void preview(int index, ImVec2 position, float scale, int opacity = 255, bool peace = false) {
    const auto pixels = character_portrait(index, peace);
    ImDrawList *draw = ImGui::GetForegroundDrawList();
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) {
            const uint32_t pixel = pixels[y * 32 + x];
            if (!pixel) continue;
            draw->AddRectFilled(ImVec2(std::floor(position.x + x * scale), std::floor(position.y + y * scale)),
                ImVec2(std::floor(position.x + (x + 1) * scale), std::floor(position.y + (y + 1) * scale)),
                IM_COL32((pixel >> 16) & 255, (pixel >> 8) & 255, pixel & 255, opacity));
        }
}

// SMB3 has no save. Like SMB1 Remastered's campaign file, arriving on the world map writes a
// resume point beside the executable; the title then offers Continue and New game.
int campaign_save_world = -1; // 0-based world of the saved campaign, or -1 when there is none.
int campaign_save_players = 1, map_frames = 0;

std::filesystem::path campaign_save_path() { return game_directory / "saves" / "campaign.cycstate"; }

void read_campaign_save_info() {
    campaign_save_world = -1;
    std::ifstream info(campaign_save_path().replace_extension(".ini"));
    std::string line;
    int world = -1, players = 1;
    while (std::getline(info, line)) {
        if (!line.compare(0, 6, "World=")) integer(line.c_str() + 6, 0, 7, world);
        if (!line.compare(0, 8, "Players=")) integer(line.c_str() + 8, 1, 3, players); // 3: co-op
    }
    std::error_code error;
    if (world >= 0 && std::filesystem::is_regular_file(campaign_save_path(), error)) {
        campaign_save_world = world;
        campaign_save_players = players;
    }
}

bool write_file(const std::filesystem::path &path, const void *data, size_t size) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file.write(static_cast<const char *>(data), static_cast<std::streamsize>(size))) return false;
    }
    std::filesystem::rename(temporary, path, error);
    return !error;
}

void autosave_campaign() {
    const bool on_map = frontend.started && practice_on_map() && !practice_campaign.data && !practice_target;
    if (!on_map || !cyc_host_saves_enabled() || game_directory.empty()) { map_frames = 0; return; }
    if (++map_frames != 45) return; // Once per visit, after the map has settled.
    PracticeState state;
    if (!state.capture()) return;
    const int world = cyc_cpu_ram()[0x727] & 7; // World_Num
    const int players = smb3::coop.enabled ? 3 : frontend.players;
    const std::string info = "World=" + std::to_string(world) + "\nPlayers=" + std::to_string(players) + "\n";
    if (write_file(campaign_save_path(), state.data.get(), state.size) &&
        write_file(campaign_save_path().replace_extension(".ini"), info.data(), info.size())) {
        campaign_save_world = world;
        campaign_save_players = players;
    } else std::fprintf(stderr, "SMB3 Remastered: no se pudo guardar la campaña en %s\n", campaign_save_path().string().c_str());
}

bool load_campaign_save() {
    const auto data = read_file(campaign_save_path(), 64 * 1024 * 1024);
    char error[256]{};
    if (data.empty() || !smb3_ui_restore(data.data(), data.size(), error, sizeof error)) {
        std::fprintf(stderr, "SMB3 Remastered: %s\n", *error ? error : "partida guardada no disponible");
        return false;
    }
    practice_campaign.data.reset();
    cyc_state_ui_reset();
    smb3::coop.enabled = 0; // Player 2 joins again with Select (smb3_coop_join).
    frontend.players = campaign_save_players == 3 ? 1 : campaign_save_players;
    return true;
}

bool restart_campaign() {
    practice_campaign.data.reset();
    if (!practice_boot.restore()) return false;
    cyc_state_ui_reset();
    frontend.started = false;
    return true;
}

#include "frontend.h"

#endif

void frontend_input(void *, uint8_t buttons[2]) {
    if (smb3::coop.enabled) {
        // The ROM reads controller 1 for the player in RAM; the other controller drives the partner.
        if (smb3::coop.partner_is_p1) std::swap(buttons[0], buttons[1]);
        smb3::coop.buttons = !frontend.active || frontend.screen == smb3::Screen::Playing ? buttons[1] : 0;
        buttons[1] = 0;
    }
    if (!frontend.active) return;
    if (frontend.screen == smb3::Screen::Intro) {
        const auto *ram = cyc_cpu_ram();
        buttons[0] = frontend.intro_buttons(ram[0x101], ram[0xde]);
        buttons[1] = 0;
    } else if (frontend.screen != smb3::Screen::Playing) {
#ifdef CYC_WITH_RECOMP_UI
        if (paused_wait) paused_buttons = {buttons[0], buttons[1]};
#endif
        buttons[0] = buttons[1] = 0;
    } else if (frontend.start_tick >= 0) {
        const auto *ram = cyc_cpu_ram();
        buttons[0] = frontend.startup_buttons(ram[0x101], ram[0xde], ram[0x72b]);
        buttons[1] = 0;
    }
#ifdef CYC_WITH_RECOMP_UI
    else if (practice_target) buttons[0] = buttons[1] = 0;
    else for (size_t player = 0; player < paused_buttons.size(); ++player) {
        paused_buttons[player] &= buttons[player];
        buttons[player] &= ~paused_buttons[player];
    }
#endif
}
void frontend_frame_end(void *) {
    update_screen_effects();
    smb3::coop_frame_end(cyc_cpu_ram());
#ifdef CYC_WITH_RECOMP_UI
    // The wait can last longer than the ROM's eight-bit tick. Keep it negative until resume.
    if (paused_wait && cyc_cpu_ram()[VBlankTick] == 0x80) cyc_mod_poke(VBlankTick, 0xff);
    autosave_campaign(); // At a frame boundary, where a save state is complete.
#endif
    if (frontend.active && frontend.screen == smb3::Screen::Intro) {
        ++frontend.intro_tick;
        const auto *ram = cyc_cpu_ram();
        if (frontend.intro_tick < 60 || !in_title(ram)) return;
        // The last scenery event queues the native menu after the actors finish.
        const bool menu_queued = ram[0xde] == 1 && ram[0xee] == 9;
        if (!menu_queued && ram[0xde] != 3) return;
        if (!(cyc_render_line_mask(80) & 0x18)) return;
#ifdef CYC_WITH_RECOMP_UI
        title_picture = current_picture();
        title_native_origin = cyc_render_native_origin(title_picture.width);
        frontend_screen(smb3::Screen::Main);
        cyc_ui_set_toast(nullptr, nullptr);
#else
        frontend.screen = smb3::Screen::Main;
#endif
    }
}

const CycHostOption Options[] = {
    {"--character", true, "mario | luigi | toad | toadette (apariencia)"},
    {"--graphics", true, "classic | remastered"},
    {"--aspect", true, "native | 16:9 | fit"},
    {"--skip-selection", false, "Omitir la pantalla de personajes (pruebas automatizadas)"},
    {"--coop", false, "Dos jugadores a la vez: el mando 2 controla al compañero"},
    {"--partner", true, "mario | luigi | toad | toadette (jugador 2 en cooperativo)"},
    {"--level", true, "Empezar en un nivel de práctica, p. ej. 1-4 o \"1-Airship\" (pruebas)"},
};

CycHostExtras make_extras() {
    for (int registered : ActorHooksRegistered)
        if (!registered) { std::fprintf(stderr, "No se pudo registrar la adaptacion panoramica.\n"); std::abort(); }
    CycHostExtras extras{};
    extras.view_modes = 7;
    extras.initial_fit_width = 426;
    extras.get_view_mode = view_mode;
    extras.set_view_mode = set_view;
    extras.load_setting = load_setting;
    extras.save_settings = save_settings;
    extras.power_on = power_on;
    extras.frame_begin = frame_begin;
    extras.frame_end = frontend_frame_end;
    extras.input = frontend_input;
    extras.state_loaded = state_loaded;
    extras.options = Options;
    extras.option_count = sizeof Options / sizeof Options[0];
    extras.option = option;
#ifdef CYC_WITH_RECOMP_UI
    extras.audio_mix = filter_pause_audio;
    extras.menu_items = MenuItems;
    extras.menu_item_count = sizeof MenuItems / sizeof MenuItems[0];
    extras.menu_callbacks = &MenuCallbacks;
    extras.ui_is_open = ui_open;
    extras.ui_render = ui_render;
#endif
    return extras;
}
}

extern "C" const CycHostExtras *cyc_host_extras(void) {
    static const CycHostExtras extras = make_extras();
    return &extras;
}

extern "C" bool smb3_ui_spanish(void) { return spanish_ui; }
extern "C" bool smb3_coop_enabled(void) { return smb3::coop.enabled; }

#ifdef CYC_WITH_RECOMP_UI
// Player 2 pressed Select (src/ui_host.c): in a one-player game they join beside player 1, who keeps the
// lead, as in Super Mario Bros. Wonder. SMB3's own two-player game takes turns, so it stays as it is.
extern "C" void smb3_coop_join(void) {
    if (frontend.players != 1 || (frontend.active && frontend.screen != smb3::Screen::Playing)) return;
    smb3::coop.enabled = 1;
    smb3::coop.partner_is_p1 = 0;
}

const char *smb3_launcher_i18n_text(const char *english) { return english ? ui_label(english) : ""; }

extern "C" void smb3_ui_prepare_fonts(void) { frontend_fonts(); }
extern "C" int smb3_ui_event(const SDL_Event *event) { return frontend_event(event); }
extern "C" bool smb3_host_menu_open(void) {
    if ((frontend.screen == smb3::Screen::Intro || paused_wait) && !cyc_state_ui_is_open()) return false;
    return cyc_ui_menu_open();
}
extern "C" void smb3_toggle_host_menu(void) {
    if (!frontend.active || cyc_state_ui_is_open()) cyc_ui_toggle_menu();
    else if (frontend.screen == smb3::Screen::Playing) frontend_pause();
    else frontend_back();
}
extern "C" int smb3_ui_owns_input(void) { return frontend.active && !cyc_state_ui_is_open(); }
extern "C" int smb3_ui_receives_events(void) {
    return !frontend.active || ui_open(nullptr) || recomp_runtime_ui_is_open(cyc_ui_runtime()) || cyc_state_ui_is_open();
}
extern "C" void smb3_render_runtime_menu(RecompRuntimeUi *model) {
    // Keep one presentation and one input cursor over the shared settings model.
    if (!frontend.active || !recomp_runtime_ui_is_open(model)) recomp_runtime_ui_render_imgui(model);
}

extern "C" void smb3_render_state_menu(void) {
    if (!cyc_state_ui_is_open()) return;
    const ImVec2 size = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), size, IM_COL32(41, 40, 35, 185));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(19, 18));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleColor(ImGuiCol_Text, ui_color(MenuWhite));
    ImGui::PushStyleColor(ImGuiCol_Border, ui_color(0x655747));
    ImGui::PushStyleColor(ImGuiCol_Separator, ui_color(MenuTrim));
    ImGui::PushStyleColor(ImGuiCol_Button, ui_color(0x634727));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ui_color(0x89602f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ui_color(0x9c6427));
    ImGui::PushStyleColor(ImGuiCol_Header, ui_color(0x634727));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ui_color(0x89602f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ui_color(0x9c6427));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ui_color(0x3f3329));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ui_color(MenuGold));
    if (ui_regular) ImGui::PushFont(ui_regular);
    cyc_state_ui_render();
    if (ui_regular) ImGui::PopFont();
    ImGui::PopStyleColor(11);
    ImGui::PopStyleVar(5);
}

namespace smb3_state_style {
namespace {
const char *label_for(const char *label) {
    if (!std::strcmp(label, "Save-state browser opened") || !std::strcmp(label, "Rewind opened")) return "";
    if (!smb3_ui_spanish()) return label;
    static const char *const names[][2] = {
        {"Save slot", "Guardar"}, {"Load slot", "Cargar"}, {"Close", "Volver"},
        {"Resume here", "Reanudar aquí"}, {"History", "Historial"}, {"Empty", "Vacío"},
        {"%.1f seconds ago", "Hace %.1f s"},
        {"Enter / A: load   S / X: save   Escape / B: close", "Enter / A: cargar    S / X: guardar    Esc / B: volver"},
        {"Left / Right: choose a moment   Enter / A: resume   Escape / B: cancel", "Izq / Der: tiempo    Enter / A: reanudar    Esc / B: volver"},
        {"Play a little longer to build rewind history.", "Juega un momento para crear el historial."},
        {"Rewind is disabled. Enable it in the launcher's settings.", "Activa el rebobinado en Configuración."},
        {"State saved", "Partida guardada"}, {"No readable state in this slot", "No hay partida en esta ranura"},
        {"Could not write this save slot", "No se pudo guardar la partida"},
        {"State loaded", "Partida cargada"}, {"Resumed from rewind", "Partida reanudada"},
        {"History entry unavailable", "Momento del historial no disponible"},
        {"Could not retain rewind snapshot", "No se pudo guardar el momento en el historial"},
        {"Rewind snapshot exceeds the memory limit", "El momento del historial supera el límite de memoria"},
        {"Save states are unavailable during online play", "Las partidas guardadas no están disponibles en línea"},
        {"Save states and rewind are unavailable during online play", "Las partidas guardadas y el rebobinado no están disponibles en línea"},
        {"Closed without loading a state", "Cerrado sin cargar una partida"},
        {"out of memory", "Memoria insuficiente"}, {"not a save state", "El archivo no es una partida guardada"},
        {"truncated save state", "La partida guardada está incompleta"},
        {"save state of another version", "La partida guardada pertenece a otra versión"},
        {"save state of another program", "La partida guardada pertenece a otro juego"},
        {"save state of another build", "La partida guardada pertenece a otra compilación"},
        {"save state sections do not match this machine", "La partida guardada es incompatible con esta máquina"},
        {"save state's host section does not match this host", "La partida guardada es incompatible con este ejecutable"},
    };
    for (const auto &pair : names) if (!std::strcmp(label, pair[0])) return pair[1];
    return ui_label(label);
}
}

bool Begin(const char *title, bool *open, ImGuiWindowFlags flags) {
    const bool shown = ImGui::Begin(title, open, flags | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavInputs);
    if (shown) {
        const ImVec2 pos = ImGui::GetWindowPos(), size = ImGui::GetWindowSize();
        auto *draw = ImGui::GetWindowDrawList();
        draw->PushClipRectFullScreen();
        ui_frame(pos, ImVec2(pos.x + size.x, pos.y + size.y), std::min(1.5f, ui_scale(ImGui::GetIO().DisplaySize)), draw);
        draw->PopClipRect();
        ImGui::SetWindowFontScale(ui_regular ? 2.2f : 1.1f);
        ImGui::TextColored(ImVec4(.96f, .82f, .53f, 1), "%s", ui_label(title));
        ImGui::SetWindowFontScale(ui_regular ? 1.5f : 1.f);
    }
    return shown;
}
void PushFont(ImFont *font) { ImGui::PushFont(ui_regular ? ui_regular : font); }
void Text(const char *format, ...) {
    va_list args;
    va_start(args, format);
    ImGui::TextV(label_for(format), args);
    va_end(args);
}
void TextUnformatted(const char *text) { ImGui::TextUnformatted(label_for(text)); }
void TextWrapped(const char *format, ...) {
    va_list args;
    va_start(args, format);
    ImGui::PushTextWrapPos(0);
    if (std::strcmp(format, "%s") == 0) ImGui::TextUnformatted(label_for(va_arg(args, const char *)));
    else ImGui::TextV(label_for(format), args);
    ImGui::PopTextWrapPos();
    va_end(args);
}
bool Button(const char *label, const ImVec2 &size) { return ImGui::Button(label_for(label), size); }
bool Selectable(const char *label, bool selected, ImGuiSelectableFlags flags, const ImVec2 &size) {
    char translated[24];
    if (smb3_ui_spanish() && std::strncmp(label, "Slot ", 5) == 0) {
        std::snprintf(translated, sizeof translated, "Ranura %.2s", label + 5);
        label = translated;
    }
    return ImGui::Selectable(label, selected, flags, size);
}
bool SliderInt(const char *label, int *value, int minimum, int maximum, const char *format) {
    return ImGui::SliderInt(label_for(label), value, minimum, maximum, format);
}
}
#endif
