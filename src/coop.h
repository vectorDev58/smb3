#pragma once
// Simultaneous two-player co-op. The ROM has one player per level, so the partner's player
// variables live here and are swapped into RAM to run the ROM's own Player_Update with
// controller 2, right after the lead's. Each object meets whichever player is nearer, so
// stomps, kicks, power-ups and damage use the original object code. The player in RAM leads:
// the camera follows it, and a partner who takes a pipe or door, grabs the goal or outlives
// the lead becomes the lead.
#include "pixels.h"
#include "cyc_core.h"
#include "cyc_mod.h"
#include "cyc_video.h"
#include <algorithm>
#include <array>
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
enum CoopIndex { PadHolding = 0, PadInput = 1, XHi = 2, YHi = 3, X = 4, Y = 5, XVel = 8, YVel = 9, Suit = 15,
                 Flip = 17, Dying = 19, FlashInv = 34 };
static_assert(CoopRam[XHi] == 0x75 && CoopRam[Y] == 0xa2 && CoopRam[Suit] == 0xed && CoopRam[Flip] == 0xef &&
              CoopRam[Dying] == 0xf1 && CoopRam[FlashInv] == 0x552, "CoopIndex must match CoopRam");
enum class CoopPass : uint8_t { Idle, Lead, Partner };

struct CoopState { // Saved in save states as raw bytes: keep it trivially copyable.
    uint8_t enabled = 0, present = 0, partner_is_p1 = 0, respawn = 0, buttons = 0, previous = 0;
    uint8_t lead_died = 0, partner_died = 0, swapped = 0, pressed = 0;
    CoopPass pass = CoopPass::Idle;
    std::array<uint8_t, CoopRamSize> partner{};
    std::array<uint8_t, 24> sprites{}, saved_sprites{};
    std::array<uint8_t, 6> banks{}, saved_banks{};
    uint8_t saved_event = 0, transitions = 0;
    std::array<uint8_t, 3> saved_palette{};
    int scroll_x = 0, scroll_y = 0;
};
inline CoopState coop;
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

// A fresh partner beside the lead: after a death, at a level's start, or when left far behind.
inline void coop_place_partner(const uint8_t *ram, bool keep_suit) {
    const uint8_t suit = keep_suit ? coop.partner[Suit] : 0;
    coop.partner.fill(0);
    coop.partner[XHi] = ram[0x75];
    coop.partner[X] = ram[0x90];
    coop.partner[YHi] = ram[0x87];
    coop.partner[Y] = ram[0xa2];
    coop.partner[Suit] = suit;
    coop.partner[Flip] = ram[0xef]; // Faces like the lead
    if (!keep_suit) coop.partner[FlashInv] = 0x80; // Blinks while it returns.
    coop.present = 1;
    coop.respawn = 0;
    coop.sprites.fill(0xf8);
}

inline void coop_mark_partner_dead() {
    coop.partner[Dying] = 0;
    coop.partner[Suit] = 0;
    coop.respawn = 150; // Frames before it returns beside the lead.
    coop.sprites.fill(0xf8);
}

// The camera follows the lead, so the partner stays within the NES screen it shows. There the
// ROM's camera can follow either player, which matters when the partner takes the lead.
inline void coop_keep_in_view(const uint8_t *ram) {
    if (ram[0x3ef]) return; // Vertical rooms scroll for both players' screen columns already.
    const int camera = ram[0x12] * 256 + ram[0xfd];
    const int left = camera + 8, right = std::min((ram[0x22] + 1) * 256, camera + 256) - 24;
    const int x = coop_x(coop.partner.data()), clamped = std::clamp(x, left, right);
    if (x == clamped) return;
    coop.partner[XHi] = uint8_t(clamped >> 8);
    coop.partner[X] = uint8_t(clamped);
    coop.partner[XVel] = 0;
}

// Player_Update entry. Its first run is the lead's; a pushed return address runs it again
// for the partner, who then has the RAM, controller 2 and its own sprites.
inline int coop_player_update(const uint8_t *ram) {
    if (!coop_in_level(ram)) {
        if (coop.present && !coop.respawn) coop.partner[Dying] = 0;
        coop.present = 0;
        coop.pass = CoopPass::Idle;
        return 0;
    }
    if (coop.pass == CoopPass::Idle) {
        if (coop.lead_died && !coop.respawn && coop.present) {
            // The lead fell or was defeated while the partner lives: the partner takes over.
            coop_exchange();
            coop_promote(ram);
            cyc_mod_poke(0x510, 0); // Event_Countdown: no drop to the map
            coop_mark_partner_dead();
        }
        coop.lead_died = 0;
        coop.pressed = 0;
        if (!coop.present) coop_place_partner(ram, true);
        if (coop.respawn && --coop.respawn == 0) coop_place_partner(ram, false);
        const int dx = std::abs(coop_x(coop.partner.data()) - (ram[0x75] * 256 + ram[0x90]));
        const int dy = std::abs(coop_y(coop.partner.data()) - (int8_t(ram[0x87]) * 256 + ram[0xa2]));
        if (!coop.respawn && (dx > cyc_video_width() || dy > 224)) coop_place_partner(ram, true);
        // The partner waits while the lead dies, grows, shrinks, uses a pipe or ends the level.
        if (coop.respawn || ram[0xf1] || ram[0xce] || ram[0x571] || ram[0x559] || ram[0x5f2]) return 0;
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
                coop_mark_partner_dead();
            }
            if (palette_changed) coop_queue_palette(ram); // The partner's suit change set its palette.
            coop_keep_in_view(ram);
        }
    }
    coop.pass = CoopPass::Idle;
    return 0;
}

// In Gameplay_UpdateAndDrawMisc, before the lead's own check: Fire and Hammer partners throw
// with B through PlayerProj_ThrowWeapon, under the same conditions as the lead.
inline int coop_throw(const uint8_t *ram) {
    if (!coop_in_level(ram) || !coop.present || coop.respawn || !(coop.pressed & 0x40) || ram[0xce]) return 0;
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
    if (!coop.present || coop.respawn || time_up) return 0;
    coop.lead_died = 1;
    return 1;
}

// Before and after each object's turn: the nearer player is the one in RAM.
inline void coop_object_begin(const uint8_t *ram) {
    if (coop.swapped) { coop_exchange(); coop.swapped = 0; }
    if (!coop_in_level(ram) || coop.pass != CoopPass::Idle || !coop.present || coop.respawn) return;
    const int slot = ram[0xcd]; // SlotIndexBackup
    if (slot >= 8 || !ram[0x661 + slot]) return;
    const int x = ram[0x76 + slot] * 256 + ram[0x91 + slot], y = int8_t(ram[0x88 + slot]) * 256 + ram[0xa3 + slot];
    const int lead = std::abs(x - (ram[0x75] * 256 + ram[0x90])) + std::abs(y - (int8_t(ram[0x87]) * 256 + ram[0xa2]));
    const int partner = std::abs(x - coop_x(coop.partner.data())) + std::abs(y - coop_y(coop.partner.data()));
    if (partner >= lead) return;
    coop.transitions = coop_transitions(ram);
    coop_exchange();
    coop_partner_pads(false);
    coop.swapped = 1;
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
    if (died) coop_mark_partner_dead();
}

inline int coop_save(uint8_t *buffer, int capacity) {
    if (capacity < int(sizeof coop)) return -1;
    std::memcpy(buffer, &coop, sizeof coop);
    return int(sizeof coop);
}

// The session decides whether co-op is on (the menu, Continue or --coop); a state brings the partner.
inline int coop_load(const uint8_t *buffer, int length) {
    if (length != int(sizeof coop)) return 0;
    const uint8_t enabled = coop.enabled;
    std::memcpy(&coop, buffer, sizeof coop);
    coop.enabled = enabled;
    return 1;
}
}
