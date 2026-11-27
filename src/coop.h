#pragma once
// Simultaneous two-player co-op. The ROM has one player per level, so the partner's player
// variables live here and are swapped into RAM to run the ROM's own Player_Update with
// controller 2, right after the lead's. Each object meets whichever player is nearer, so
// stomps, kicks, power-ups and damage use the original object code. The player in RAM leads:
// the camera follows it, and a partner who takes a pipe or door, grabs the goal or outlives
// the lead becomes the lead. As in Super Mario Bros. Wonder, a defeated player is a ghost the
// lead can touch for a few seconds, and then floats back in a bubble, as does one left behind.
#include "actors.h"
#include "pixels.h"
#include "cyc_core.h"
#include "cyc_mod.h"
#include "cyc_video.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace smb3 {
// Per-player RAM from Southbird's disassembly. Pad_Holding and Pad_Input come first.
inline constexpr uint16_t CoopRam[] = {
    0x17, 0x18, 0x75, 0x87, 0x90, 0xa2, 0xab, 0xb4, 0xbd, 0xcf, 0xd8, 0xe6, 0xe7, 0xe8, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0xf1, 0x3dd, 0x41f, 0x420, 0x511, 0x512, 0x513, 0x515, 0x517, 0x544, 0x545, 0x546, 0x54f, 0x550,
    0x551, 0x552, 0x553, 0x554, 0x555, 0x556, 0x557, 0x558, 0x559, 0x569, 0x56b, 0x56c, 0x56d, 0x56e, 0x56f,
    0x570, 0x573, 0x574, 0x575, 0x576, 0x577, 0x578, 0x579, 0x57a, 0x57b, 0x57c, 0x57d, 0x57e, 0x583, 0x585,
    0x587, 0x588, 0x589, 0x58a, 0x58b, 0x58c, 0x58d, 0x6a4, 0x6a5, 0x6b6, 0x74d, 0x75f};
constexpr size_t CoopRamSize = sizeof CoopRam / sizeof CoopRam[0];
constexpr int coop_index(uint16_t address) {
    for (size_t i = 0; i < CoopRamSize; ++i) if (CoopRam[i] == address) return int(i);
    return -1;
}
constexpr uint16_t CoopPlayerUpdate = 0xA3CA;
enum CoopIndex { PadHolding = 0, PadInput = 1, XHi = 2, YHi = 3, X = 4, Y = 5, SpriteY = 7, XVel = 8, YVel = 9,
                 Suit = 15, Flip = 17, Dying = 19, AboveTop = 28, FlashInv = 34 };
static_assert(CoopRam[XHi] == 0x75 && CoopRam[Y] == 0xa2 && CoopRam[SpriteY] == 0xb4 && CoopRam[Suit] == 0xed &&
              CoopRam[Flip] == 0xef && CoopRam[Dying] == 0xf1 && CoopRam[AboveTop] == 0x544 &&
              CoopRam[FlashInv] == 0x552, "CoopIndex must match CoopRam");
constexpr int CoopGhostFrames = 300; // Wonder gives a ghost five seconds.
enum class CoopPass : uint8_t { Idle, Lead, Partner };

struct CoopState { // Saved in save states as raw bytes: keep it trivially copyable.
    uint8_t enabled = 0, present = 0, partner_is_p1 = 0, bubble = 0, buttons = 0, previous = 0;
    uint8_t lead_died = 0, partner_died = 0, swapped = 0, pressed = 0;
    uint16_t ghost = 0; // Frames left as a ghost
    int away_x = 0, away_y = 0; // World centre of the ghost or bubble
    CoopPass pass = CoopPass::Idle;
    std::array<uint8_t, CoopRamSize> partner{};
    std::array<uint8_t, 24> sprites{}, saved_sprites{};
    std::array<uint8_t, 6> banks{}, saved_banks{};
    uint8_t saved_event = 0, transitions = 0;
    std::array<uint8_t, 3> saved_palette{};
    int scroll_x = 0, scroll_y = 0;
};
inline CoopState coop;
// tools/verify_widescreen.py reads these from save states.
static_assert(offsetof(CoopState, present) == 1 && offsetof(CoopState, partner_is_p1) == 2 &&
              offsetof(CoopState, bubble) == 3 && offsetof(CoopState, buttons) == 4 &&
              offsetof(CoopState, ghost) == 10 && offsetof(CoopState, partner) == 21 && CoopRam[X] == 0x90,
              "Update coop_state()");
inline const uint8_t *coop_palettes = nullptr; // PowerUp_Palettes in the ROM: 8 rows of 4 colors.

// Level_SetPlayerPUpPal for the player in RAM: the player palette follows the lead's suit.
inline void coop_queue_palette(const uint8_t *ram) {
    const int suit = ram[0xed], row = ram[0x57a] ? 7 : suit < 2 || suit == 3 ? 0 : suit;
    const int at = ram[0x300]; // Graphics_BufCnt
    if (!coop_palettes || row > 7 || at > 0x70) return;
    const uint8_t *colors = coop_palettes + row * 4;
    const uint8_t entry[] = {0x3f, 0x11, 0x03, colors[1], colors[2], colors[3], 0x00};
    for (int i = 0; i < 7; ++i) cyc_mod_poke(0x301 + at + i, entry[i]);
    for (int i = 0; i < 3; ++i) cyc_mod_poke(0x7d2 + i, colors[1 + i]); // Palette_Buffer+$11
    cyc_mod_poke(0x300, at + 6);
}

inline int coop_x(const uint8_t *player) { return player[XHi] * 256 + player[X]; }
inline int coop_y(const uint8_t *player) { return int8_t(player[YHi]) * 256 + player[Y]; }

inline bool coop_in_level(const uint8_t *ram) {
    return coop.enabled && ram[0x70a] > 0 && ram[0x70a] < 15 && ram[0x101] != 0x20;
}

inline void coop_exchange() {
    for (size_t i = 0; i < CoopRamSize; ++i) {
        const uint8_t value = cyc_mod_peek(CoopRam[i]);
        cyc_mod_poke(CoopRam[i], coop.partner[i]);
        coop.partner[i] = value;
    }
}

inline void coop_partner_pads(bool newly_pressed) {
    cyc_mod_poke(0x17, coop.buttons); // Pad_Holding
    cyc_mod_poke(0x18, newly_pressed ? coop.buttons & ~coop.previous : 0); // Pad_Input
}

// Globals a pipe, door, vine, goal or exit changes: whoever set them must lead.
inline uint8_t coop_transitions(const uint8_t *ram) {
    return (ram[0x571] != 0) | (ram[0x3de] != 0) << 1 | (ram[0x14] != 0) << 2 | (ram[0x559] != 0) << 3;
}

// The sprites and CHR banks of the player in RAM trade places with the partner's.
inline void coop_exchange_sprites(const uint8_t *ram) {
    const int first = ram[0x58e] & 0xfc; // Player_SprOff
    for (int i = 0; i < 24 && first + i < 256; ++i) {
        const uint8_t value = ram[0x200 + first + i];
        cyc_mod_poke(0x200 + first + i, coop.sprites[i]);
        coop.sprites[i] = value;
    }
    for (int i = 0; i < 6; ++i) {
        const uint8_t value = ram[0x719 + i];
        cyc_mod_poke(0x719 + i, coop.banks[i]);
        coop.banks[i] = value;
    }
}

inline void coop_promote(const uint8_t *ram) {
    coop.partner_is_p1 ^= 1;
    coop_exchange_sprites(ram);
    coop_queue_palette(ram);
}

inline bool coop_partner_active() { return coop.present && !coop.ghost && !coop.bubble; }

// The partner beside the lead, keeping its suit: at a level's start, after a pipe, or back from
// a ghost or bubble, when it blinks for a moment.
inline void coop_place_partner(const uint8_t *ram, bool blink) {
    const uint8_t suit = coop.partner[Suit];
    coop.partner.fill(0);
    coop.partner[XHi] = ram[0x75];
    coop.partner[X] = ram[0x90];
    coop.partner[YHi] = ram[0x87];
    coop.partner[Y] = ram[0xa2];
    coop.partner[Suit] = suit;
    coop.partner[Flip] = ram[0xef]; // Faces like the lead
    if (blink) coop.partner[FlashInv] = 0x80;
    coop.present = 1;
    coop.ghost = coop.bubble = 0;
    coop.sprites.fill(0xf8);
}

// The level on screen in world pixels: the lead's view, as wide as the window shows.
struct CoopView { int left, right, top, bottom; };
inline CoopView coop_view(const uint8_t *ram) {
    const int top = ram[0x542] * 256 + ram[0x543]; // Level_VertScrollH, Level_VertScroll
    if (ram[0x3ef]) return {0, 256, top, top + 192};
    const int left = world_left(ram);
    return {std::max(0, left), std::min((ram[0x22] + 1) * 256, left + cyc_video_width()), top, top + 192};
}

// The middle of a player's 16 x 32 box, where a ghost or bubble shows it standing.
inline int coop_center_x(const uint8_t *player) { return coop_x(player) + 8; }
inline int coop_center_y(const uint8_t *player) { return coop_y(player) + 16; }

inline void coop_send_bubble() {
    coop.away_x = coop_center_x(coop.partner.data());
    coop.away_y = coop_center_y(coop.partner.data());
    coop.ghost = 0;
    coop.bubble = 1;
    coop.sprites.fill(0xf8);
}

// The partner was defeated: a ghost where it fell, or a bubble rising from a pit.
inline void coop_partner_fell() {
    const bool pit = coop.partner[SpriteY] >= 0xb0 && !coop.partner[AboveTop];
    coop.partner[Dying] = 0;
    coop.partner[Suit] = 0;
    coop_send_bubble();
    if (!pit) { coop.bubble = 0; coop.ghost = CoopGhostFrames; }
}

// A ghost waits for the lead's touch, then floats back in a bubble, which pops at the lead.
inline void coop_update_away(const uint8_t *ram) {
    const CoopView view = coop_view(ram);
    const int dx = ram[0x75] * 256 + ram[0x90] + 8 - coop.away_x;
    const int dy = int8_t(ram[0x87]) * 256 + ram[0xa2] + 16 - coop.away_y;
    const bool lead_well = !ram[0xf1];
    if (coop.ghost) {
        if (std::abs(dx) < 16 && std::abs(dy) < 24 && lead_well) { coop_place_partner(ram, true); return; }
        const bool seen = coop.away_x >= view.left - 8 && coop.away_x < view.right + 8;
        if (--coop.ghost && seen) return;
        coop.ghost = 0;
        coop.bubble = 1;
    }
    if (std::abs(dx) < 12 && std::abs(dy) < 16 && lead_well) { coop_place_partner(ram, true); return; }
    // It enters from the screen's edge, or wherever the lead is, and slows as it nears the lead.
    auto step = [](int distance) { return std::clamp(distance / 6, -5, 5) + (distance > 0) - (distance < 0); };
    const int lead_x = coop.away_x + dx, lead_y = coop.away_y + dy;
    coop.away_x = std::clamp(coop.away_x + step(dx), std::min(view.left + 16, lead_x), std::max(view.right - 16, lead_x));
    coop.away_y = std::clamp(coop.away_y + step(dy), std::min(view.top + 16, lead_y), std::max(view.bottom - 16, lead_y));
}

// The camera follows the lead. A partner who walks against a screen edge stops there, like the
// lead; one the screen leaves behind floats back in a bubble instead of being dragged into walls.
inline void coop_keep_in_view(const uint8_t *ram) {
    if (ram[0x3ef]) { coop.partner[XHi] = 0; return; } // Vertical rooms are one screen wide, as Player_DoScrolling keeps them.
    const CoopView view = coop_view(ram);
    const int left = view.left + 8, right = view.right - 24;
    const int x = coop_x(coop.partner.data()), clamped = std::clamp(x, left, right);
    if (x == clamped) return;
    const int velocity = int8_t(coop.partner[XVel]);
    if (std::abs(x - clamped) <= 4 && (x < left ? velocity < 0 : velocity > 0)) {
        coop.partner[XHi] = uint8_t(clamped >> 8);
        coop.partner[X] = uint8_t(clamped);
        coop.partner[XVel] = 0;
    } else if (std::abs(x - clamped) > 16) coop_send_bubble();
}

// Player_Update entry. Its first run is the lead's; a pushed return address runs it again
// for the partner, who then has the RAM, controller 2 and its own sprites.
inline int coop_player_update(const uint8_t *ram) {
    if (!coop_in_level(ram)) {
        coop.partner[Dying] = 0;
        coop.present = 0;
        coop.pass = CoopPass::Idle;
        return 0;
    }
    if (coop.pass == CoopPass::Idle) {
        if (coop.lead_died && coop_partner_active()) {
            // The lead fell or was defeated while the partner lives: the partner takes over.
            coop_exchange();
            coop_promote(ram);
            cyc_mod_poke(0x510, 0); // Event_Countdown: no drop to the map
            coop_partner_fell();
        }
        coop.lead_died = 0;
        coop.pressed = 0;
        if (!coop.present) coop_place_partner(ram, false);
        else if (!coop_partner_active()) coop_update_away(ram);
        else {
            const int dx = std::abs(coop_x(coop.partner.data()) - (ram[0x75] * 256 + ram[0x90]));
            const int dy = std::abs(coop_y(coop.partner.data()) - (int8_t(ram[0x87]) * 256 + ram[0xa2]));
            if (dx > cyc_video_width() || dy > 224) coop_send_bubble();
        }
        // The partner waits while the lead dies, grows, shrinks, uses a pipe or ends the level.
        if (!coop_partner_active() || ram[0xf1] || ram[0xce] || ram[0x571] || ram[0x559] || ram[0x5f2]) return 0;
        CycModRegs registers{};
        cyc_mod_regs(&registers);
        constexpr uint16_t again = CoopPlayerUpdate - 1; // RTS adds one.
        cyc_mod_poke(0x100 | registers.s, again >> 8);
        cyc_mod_poke(0x100 | uint8_t(registers.s - 1), again & 255);
        registers.s -= 2;
        cyc_mod_set_regs(&registers);
        coop.pass = CoopPass::Lead;
        return 0;
    }
    if (coop.pass == CoopPass::Lead) {
        const int first = ram[0x58e] & 0xfc;
        for (int i = 0; i < 24 && first + i < 256; ++i) coop.saved_sprites[i] = ram[0x200 + first + i];
        std::copy_n(ram + 0x719, 6, coop.saved_banks.begin());
        coop.saved_event = ram[0x510];
        std::copy_n(ram + 0x7d2, 3, coop.saved_palette.begin());
        coop.transitions = coop_transitions(ram);
        coop_exchange();
        coop.pressed = coop.buttons & ~coop.previous;
        coop_partner_pads(true);
        coop.pass = CoopPass::Partner;
    }
    return 0;
}

// The instruction after JSR Player_Update.
inline int coop_player_done(const uint8_t *ram) {
    if (coop.pass == CoopPass::Partner) {
        coop.previous = coop.buttons;
        const bool died = coop.partner_died || ram[0xf1];
        coop.partner_died = 0;
        const bool lead_now = !died && (coop_transitions(ram) & ~coop.transitions);
        // The partner's sprites and banks are in OAM; the lead's were saved before its run.
        const int first = ram[0x58e] & 0xfc;
        for (int i = 0; i < 24 && first + i < 256; ++i) coop.sprites[i] = ram[0x200 + first + i];
        std::copy_n(ram + 0x719, 6, coop.banks.begin());
        coop.scroll_x = ram[0x12] * 256 + ram[0xfd];
        coop.scroll_y = ram[0x543];
        const bool palette_changed = !std::equal(coop.saved_palette.begin(), coop.saved_palette.end(), ram + 0x7d2);
        if (lead_now) { // Keep the partner in RAM; the old lead becomes the partner.
            coop.partner_is_p1 ^= 1;
            coop.sprites = coop.saved_sprites;
            coop.banks = coop.saved_banks;
            coop_queue_palette(ram);
        } else {
            coop_exchange();
            for (int i = 0; i < 24 && first + i < 256; ++i) cyc_mod_poke(0x200 + first + i, coop.saved_sprites[i]);
            for (int i = 0; i < 6; ++i) cyc_mod_poke(0x719 + i, coop.saved_banks[i]);
            if (died) {
                cyc_mod_poke(0x510, coop.saved_event);
                coop_partner_fell();
            } else coop_keep_in_view(ram);
            if (palette_changed) coop_queue_palette(ram); // The partner's suit change set its palette.
        }
    }
    coop.pass = CoopPass::Idle;
    return 0;
}

// In Gameplay_UpdateAndDrawMisc, before the lead's own check: Fire and Hammer partners throw
// with B through PlayerProj_ThrowWeapon, under the same conditions as the lead.
inline int coop_throw(const uint8_t *ram) {
    if (!coop_in_level(ram) || !coop_partner_active() || !(coop.pressed & 0x40) || ram[0xce]) return 0;
    const auto &p = coop.partner;
    if ((p[Suit] != 2 && p[Suit] != 6) || p[coop_index(0x556)] || p[coop_index(0x56f)] || p[coop_index(0x557)] ||
        p[coop_index(0x58c)]) return 0; // Halted, ducking, kicking or in a pipe
    CycModRegs saved{}, call{};
    cyc_mod_regs(&saved);
    call = saved;
    coop_exchange();
    cyc_mod_call_commit(0xA2AF, &call); // PlayerProj_ThrowWeapon
    coop_exchange();
    cyc_mod_set_regs(&saved);
    return 0;
}

// Player_DoScrolling: the camera and its screen edges follow the lead only.
inline int coop_scrolling() { return coop.pass == CoopPass::Partner; }

// Player_Die: the partner, or a lead whose partner lives, is not the end of the level.
inline int coop_die(const uint8_t *ram) {
    if (!coop_in_level(ram)) return 0;
    if (coop.pass == CoopPass::Partner || coop.swapped) { coop.partner_died = 1; return 1; }
    const bool time_up = !(ram[0x5ee] | ram[0x5ef] | ram[0x5f0]);
    if (!coop_partner_active() || time_up) return 0;
    coop.lead_died = 1;
    return 1;
}

// Before and after each object's turn: the nearer player is the one in RAM.
inline void coop_meet_nearer(const uint8_t *ram, int x, int y) {
    if (coop.swapped) { coop_exchange(); coop.swapped = 0; }
    if (!coop_in_level(ram) || coop.pass != CoopPass::Idle || !coop_partner_active()) return;
    const int lead = std::abs(x - (ram[0x75] * 256 + ram[0x90])) + std::abs(y - (int8_t(ram[0x87]) * 256 + ram[0xa2]));
    const int partner = std::abs(x - coop_x(coop.partner.data())) + std::abs(y - coop_y(coop.partner.data()));
    if (partner >= lead) return;
    coop.transitions = coop_transitions(ram);
    coop_exchange();
    coop_partner_pads(false);
    coop.swapped = 1;
}

inline void coop_object_begin(const uint8_t *ram) {
    const int slot = ram[0xcd]; // SlotIndexBackup
    if (slot < 8 && ram[0x661 + slot])
        coop_meet_nearer(ram, ram[0x76 + slot] * 256 + ram[0x91 + slot], int8_t(ram[0x88 + slot]) * 256 + ram[0xa3 + slot]);
}

// SpecialObj_UpdateAndDraw: hammers, fireballs, boomerangs and cannonballs hurt either player.
inline void coop_special_begin(const uint8_t *ram) {
    CycModRegs registers{};
    cyc_mod_regs(&registers);
    const int slot = registers.x;
    if (slot >= 8 || !cyc_mod_peek(SpecialId + slot)) return;
    const int camera = ram[0x3ef] ? 0 : ram[0x12] * 256 + ram[0xfd], low = ram[0x5c9 + slot];
    // Only the widened view keeps a projectile's page; on the NES screen it is the camera's.
    const int x = wide_level(ram, true) ? projectile_world_x(slot, low, cyc_mod_peek(SpecialId + slot), camera + 128) :
        unwrap(low, camera + 128);
    coop_meet_nearer(ram, x, cyc_mod_peek(0x7fd5 + slot) * 256 + ram[0x5bf + slot]); // SpecialObj_YHi, YLo
}

inline void coop_object_end(const uint8_t *ram) {
    if (!coop.swapped) return;
    coop.swapped = 0;
    const bool died = coop.partner_died;
    coop.partner_died = 0;
    if (!died && (coop_transitions(ram) & ~coop.transitions)) { // Grabbed the goal: the partner leads.
        coop_promote(ram);
        return;
    }
    coop_exchange();
    if (died) coop_partner_fell();
}

// Player_Update only runs in levels: outside them, the next level or area starts the partner afresh.
inline void coop_frame_end(const uint8_t *ram) {
    if (!coop_in_level(ram)) coop.present = 0;
}

inline int coop_save(uint8_t *buffer, int capacity) {
    if (capacity < int(sizeof coop)) return -1;
    std::memcpy(buffer, &coop, sizeof coop);
    return int(sizeof coop);
}

// The session decides whether co-op is on (the menu, Continue or --coop); a state brings the partner.
// One from before co-op, or from another version of it, starts the partner afresh.
inline int coop_load(const uint8_t *buffer, int length) {
    const uint8_t enabled = coop.enabled;
    if (length == int(sizeof coop)) std::memcpy(&coop, buffer, sizeof coop);
    else coop = {};
    coop.enabled = enabled;
    return 1;
}
}
