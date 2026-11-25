#pragma once
#include "pixels.h"
#include "cyc_core.h"
#include "cyc_mod.h"
#include "cyc_video.h"

namespace smb3 {
struct ExtraSprite { int world_x = 0; uint8_t x = 0, y = 0xff; };
inline std::array<ExtraSprite, 64> extra_sprites{}, displayed_extra_sprites{};
inline std::array<uint8_t, 256> actor_sprites_before{};
inline int drawing_actor = -1;
inline bool drawing_active = false;
inline int projectile_world = 0;
inline bool updating_projectile = false;
inline bool updating_player_projectile = false;
inline int collision_world = 0, collision_screen = 0;
constexpr uint16_t SpecialId = 0x7fc6;
// Cart RAM persists in states. Preserve the low chest-history bits that the ROM reads.
inline unsigned projectile_bits(int start, int count, int replacement = -1) {
    unsigned value = 0;
    for (int i = 0; i < count; ++i) {
        const int bit = start + i;
        const uint16_t address = bit < 64 ? 0x7f25 + bit / 8 : 0x7f2e + (bit - 64) / 4;
        const unsigned mask = 1u << (bit < 64 ? bit % 8 : 4 + (bit - 64) % 4);
        const uint8_t current = cyc_mod_peek(address);
        if (current & mask) value |= 1u << i;
        if (replacement >= 0) {
            const uint8_t next = (replacement & (1u << i)) ? current | mask : current & ~mask;
            if (next != current) cyc_mod_poke(address, next);
        }
    }
    return value;
}
inline int projectile_high(int slot, int value = -1) {
    return projectile_bits(slot < 8 ? slot * 13 : 104 + (slot - 8) * 6, slot < 8 ? 5 : 4, value);
}
inline int projectile_tag(int slot, int value = -1) {
    return projectile_bits(slot < 8 ? slot * 13 + 5 : 108 + (slot - 8) * 6, slot < 8 ? 8 : 2, value);
}
inline bool wide_level(const uint8_t *ram, bool enabled) {
    return enabled && cyc_video_width() > 256 && ram[0x70a] > 0 && ram[0x70a] < 15 &&
           !ram[0x3ef];
}

inline int world_left(const uint8_t *ram) {
    const int camera = ram[0x12] * 256 + ram[0xfd];
    size_t size = 0;
    const uint8_t *cart = cyc_cart_ram(&size);
    return camera - level_origin(camera, cyc_video_width(), std::min<int>(ram[0x22] + 1, 15) * 256,
                                 goal_position(cart, size));
}

inline int keep_visible_actor(const uint8_t *ram, bool enabled) {
    if (!wide_level(ram, enabled)) return 0;
    CycModRegs registers{};
    cyc_mod_regs(&registers);
    const int slot = registers.x;
    if (slot >= 8 || ram[0x88 + slot] >= 2) return 0;
    const int x = ram[0x76 + slot] * 256 + ram[0x91 + slot];
    // Vertical clipping must not send a visible actor through the NES horizontal despawner.
    return x >= world_left(ram) - 48 && x < world_left(ram) + cyc_video_width() + 48;
}

inline int remember_collision_box(const uint8_t *ram, bool enabled) {
    if (!wide_level(ram, enabled) || cyc_mod_isolated()) return 0;
    CycModRegs registers{};
    cyc_mod_regs(&registers);
    if (registers.x < 8) {
        collision_world = ram[0x76 + registers.x] * 256 + ram[0x91 + registers.x];
        collision_screen = ram[0xac + registers.x];
    }
    return 0;
}

inline int intersect_world_boxes(const uint8_t *ram, bool enabled) {
    if (!wide_level(ram, enabled) || cyc_mod_isolated()) return 0;
    CycModRegs registers{};
    cyc_mod_regs(&registers);
    if (registers.x >= 8) return 0;
    const int caller = cyc_mod_peek(0x100 | uint8_t(registers.s + 1)) |
                       (cyc_mod_peek(0x100 | uint8_t(registers.s + 2)) << 8);
    int world = collision_world, screen = collision_screen;
    if (caller == 0xd881 || (caller == 0xdb58 && ram[3] == 10)) {
        world = ram[0x75] * 256 + ram[0x90];
        screen = ram[0xab];
    } else if (caller != 0xcc52 && caller != 0xdc2a && caller != 0xb7c6 &&
               caller != 0xb45f && caller != 0xaccd) return 0;
    const int slot = registers.x;
    const int other = ram[0x76 + slot] * 256 + ram[0x91 + slot];
    const int difference = world + unwrap(ram[2], screen) - screen -
        (other + unwrap(ram[0], ram[0xac + slot]) - ram[0xac + slot]);
    if (std::abs(difference) >= 128) {
        registers.p &= ~1;
        cyc_mod_set_regs(&registers);
        return 1;
    }
    // Run the original box math away from its eight-bit wrap; retain its vertical rules.
    const uint8_t first = ram[0], second = ram[2];
    cyc_mod_poke(0, 96);
    cyc_mod_poke(2, uint8_t(96 + difference));
    const bool done = cyc_mod_call_commit(0xd97b, &registers);
    cyc_mod_poke(0, first);
    cyc_mod_poke(2, second);
    return done;
}

inline int shell_block_boundary(const uint8_t *ram, bool enabled) {
    if (!wide_level(ram, enabled)) return 0;
    CycModRegs registers{};
    cyc_mod_regs(&registers);
    registers.a = 128; // Only the legacy screen-edge guard reads A; tile coordinates stay intact.
    cyc_mod_set_regs(&registers);
    return 0;
}

inline int player_crush_boundary(const uint8_t *ram, bool enabled) {
    if (!wide_level(ram, enabled)) return 0;
    CycModRegs registers{};
    cyc_mod_regs(&registers);
    const int x = ram[0x75] * 256 + ram[0x90];
    registers.a = x < std::max(0, world_left(ram)) - 8 ? 248 : 128;
    cyc_mod_set_regs(&registers);
    return 0;
}

inline int player_window_boundary(const uint8_t *ram, bool enabled) {
    if (!wide_level(ram, enabled) || ram[0x559]) return 0;
    CycModRegs registers{};
    cyc_mod_regs(&registers);
    const int previous = ram[0x75] * 256 + ram[0x90];
    const int left = std::max(0, world_left(ram)) + 16;
    const int right = std::min((ram[0x22] + 1) * 256, world_left(ram) + cyc_video_width()) - 24;
    const int x = std::clamp(previous, left, right);
    if (x != previous) {
        cyc_mod_poke(0x75, x >> 8);
        cyc_mod_poke(0x90, x & 255);
        const int velocity = int8_t(ram[0xbd]);
        cyc_mod_poke(0xbd, velocity < 0 ? velocity | 0xf0 : velocity & 15);
    }
    registers.a = 128;
    cyc_mod_set_regs(&registers);
    return 0;
}

inline int autoscroll_player(const uint8_t *ram, bool enabled) {
    if (!wide_level(ram, enabled)) return 0;
    const int velocity = int8_t(cyc_mod_peek(0x7a0e));
    const int carry = int8_t(cyc_mod_peek(0x7a12));
    int player_velocity = int8_t(ram[0xbd]);
    int x = ram[0x75] * 256 + ram[0x90];
    const bool left = x < std::max(0, world_left(ram)) + 16;
    const bool right = x >= std::min((ram[0x22] + 1) * 256, world_left(ram) + cyc_video_width()) - 32;
    if ((left && player_velocity < velocity) || (right && player_velocity >= velocity)) {
        player_velocity = velocity;
        cyc_mod_poke(0xbd, uint8_t(velocity));
        cyc_mod_poke(0x74d, 0);
    } else if (!left && !right && !cyc_mod_peek(0x7a14)) x += carry;
    cyc_mod_poke(0x7a07, velocity < 0);
    if (player_velocity && (player_velocity < 0) == (velocity < 0) && carry) {
        x += carry;
        cyc_mod_poke(0x74d, 0);
    }
    cyc_mod_poke(0x75, uint8_t(x >> 8));
    cyc_mod_poke(0x90, uint8_t(x));
    return 1;
}

inline int actor_drawing(const uint8_t *ram, bool enabled, bool begin) {
    if (!wide_level(ram, enabled) || cyc_mod_isolated()) return 0;
    const int actor = ram[0xcd];
    if (begin) {
        drawing_actor = actor < 8 ? actor : -1;
        drawing_active = drawing_actor >= 0 && ram[0x661 + actor] != 0;
        std::copy_n(ram + 0x200, actor_sprites_before.size(), actor_sprites_before.begin());
    } else if (actor == drawing_actor && drawing_actor >= 0) {
        const int camera = ram[0x12] * 256 + ram[0xfd];
        const int world = ram[0x76 + actor] * 256 + ram[0x91 + actor];
        for (int sprite = 0; sprite < 64; ++sprite) {
            const uint8_t *current = ram + 0x200 + sprite * 4;
            if (current[0] >= 192 || std::equal(current, current + 4, actor_sprites_before.data() + sprite * 4)) continue;
            extra_sprites[sprite] = {camera + unwrap(current[3], world - camera), current[3], current[0]};
        }
        const int index = ram[0x659 + actor];
        if (drawing_active && !ram[0x661 + actor] && index < 48) {
            const uint8_t flags = cyc_mod_peek(0x7b10 + index);
            // Bit 6 is unused by the ROM; keep a fallen actor dormant until its origin leaves view.
            if (!(flags & 0x80)) cyc_mod_poke(0x7b10 + index, flags | 0xc0);
        }
        drawing_actor = -1;
    }
    return 0;
}

inline void spawn_visible_actors(const uint8_t *ram, bool enabled) {
    if (ram[0x5b] > 1) return;
    const bool wide = wide_level(ram, enabled);
    CycModRegs registers{};
    cyc_mod_regs(&registers);
    const int caller = cyc_mod_peek(0x100 | uint8_t(registers.s + 1)) |
                       (cyc_mod_peek(0x100 | uint8_t(registers.s + 2)) << 8);
    if (caller < 0xc000) return; // The initial room loader temporarily sweeps a fake camera.
    size_t size = 0;
    const uint8_t *cart = cyc_cart_ram(&size);
    if (!cart || size < 0x1bd1) return;
    const int left = std::max(0, world_left(ram) - 32);
    const int right = world_left(ram) + cyc_video_width() + 16;
    for (int index = 0; index < 48; ++index) {
        // $7B40 is the object-set header; triples start one byte later.
        const uint8_t *entry = cart + 0x1b41 + index * 3;
        if (entry[0] == 0xff) break;
        const int x = entry[1] * 16;
        if (cart[0x1b10 + index] & 0x40) {
            if (!wide || x < left || x >= right)
                cyc_mod_poke(0x7b10 + index, cart[0x1b10 + index] & 0x3f);
            else continue;
        }
        if (!wide) continue;
        // Scripted triggers and the goal card keep their original activation point.
        if ((entry[0] >= 0xb4 && (entry[0] < 0xbc || entry[0] > 0xd0)) ||
            entry[0] == 0x41 || (cart[0x1b10 + index] & 0x80)) continue;
        if (x < left || x >= right) continue;
        if (entry[0] >= 0xbc) {
            bool free = false;
            for (int slot = 0; slot < 8; ++slot) free |= cyc_mod_peek(0x7a15 + slot) == 0;
            if (!free) continue;
            cyc_mod_poke(0, x & 255);
            cyc_mod_poke(1, index);
            cyc_mod_poke(6, x >> 8);
            CycModRegs call = registers;
            call.a = entry[0] - 0xbc + 1;
            call.y = index * 3 + 2;
            if (!cyc_mod_call_commit(0xba46, &call)) return;
            continue;
        }
        int slot = 4;
        while (slot >= 0 && ram[0x661 + slot]) --slot;
        if (slot < 0) continue;
        cyc_mod_poke(0x91 + slot, x & 255);
        cyc_mod_poke(0x76 + slot, x >> 8);
        cyc_mod_poke(0xa3 + slot, entry[2] << 4);
        cyc_mod_poke(0x88 + slot, entry[2] >> 4);
        cyc_mod_poke(0x671 + slot, entry[0]);
        cyc_mod_poke(0x659 + slot, index);
        cyc_mod_poke(0x7b10 + index, cart[0x1b10 + index] | 0x80);
        cyc_mod_poke(0x661 + slot, 1);
    }
}

inline int wide_visibility(const uint8_t *ram, bool enabled) {
    if (!wide_level(ram, enabled)) return 0;
    CycModRegs registers{};
    cyc_mod_regs(&registers);
    const int slot = registers.x, columns = registers.y;
    if (slot >= 8 || columns > 5 || ram[0x661 + slot] < 2) return 0;
    const int x = ram[0x76 + slot] * 256 + ram[0x91 + slot];
    const int left = world_left(ram) - 48, right = left + cyc_video_width() + 96;
    uint8_t hidden = 0;
    for (int column = 0; column <= columns; ++column)
        if (x + column * 8 < left || x + column * 8 >= right) hidden |= 0x80 >> column;
    cyc_mod_poke(0x651 + slot, hidden);
    cyc_mod_poke(0x0e, x & 255);
    cyc_mod_poke(0x0f, x >> 8);
    registers.a = (hidden & 0x80) ? hidden : 0;
    registers.y = 0xff;
    registers.p = (registers.p & ~0x83) | 0x80 | (x >= ram[0x12] * 256 + ram[0xfd]);
    cyc_mod_set_regs(&registers);
    return 1;
}

inline int projectile_origin(const uint8_t *ram, bool enabled) {
    if (!wide_level(ram, enabled) || cyc_mod_isolated()) return 0;
    CycModRegs registers{};
    cyc_mod_regs(&registers);
    const int parent = registers.x, slot = registers.y;
    if (parent < 8 && slot < 8 && !ram[0x651 + parent] && !ram[0x681 + parent]) {
        projectile_high(slot, 0x10 | ram[0x76 + parent]);
        projectile_tag(slot, ram[0x91 + parent]);
    }
    return 0;
}

inline int projectile_hit(const uint8_t *ram) {
    return updating_projectile && std::abs(projectile_world - (ram[0x75] * 256 + ram[0x90])) >= 64;
}

inline int player_projectile_hit(const uint8_t *ram) {
    if (!updating_player_projectile) return 0;
    CycModRegs registers{};
    cyc_mod_regs(&registers);
    return registers.y < 8 && std::abs(projectile_world -
        (ram[0x76 + registers.y] * 256 + ram[0x91 + registers.y])) >= 64;
}

inline int update_projectile(const uint8_t *ram, bool enabled, bool player = false) {
    if (!wide_level(ram, enabled) || cyc_mod_isolated()) return 0;
    CycModRegs registers{};
    cyc_mod_regs(&registers);
    const int slot = registers.x;
    if (slot >= (player ? 2 : 8)) return 0;
    const uint16_t id_address = (player ? 0x7ce1 : SpecialId) + slot;
    const uint16_t x_address = (player ? 0x7ce5 : 0x5c9) + slot;
    const int meta = slot + (player ? 8 : 0);
    const uint8_t id = cyc_mod_peek(id_address);
    if (!id) { projectile_tag(meta, 0); projectile_high(meta, 0); return 0; }
    const int camera = ram[0x12] * 256 + ram[0xfd], low = cyc_mod_peek(x_address);
    const int high = projectile_high(meta), tag = projectile_tag(meta);
    const int world = high & 0x10 ? unwrap(low, (high & 15) * 256 + tag) :
        tag == id && high < 16 ? high * 256 + low : unwrap(low,
            player ? ram[0x75] * 256 + ram[0x90] : camera + 128);
    if (world < world_left(ram) - 32 || world >= world_left(ram) + cyc_video_width() + 32) {
        cyc_mod_poke(id_address, 0);
        projectile_tag(meta, 0);
        return 1;
    }
    std::array<uint8_t, 256> before{};
    std::copy_n(ram + 0x200, before.size(), before.begin());
    const int local_camera = std::max(0, world - 128);
    std::array<uint8_t, 8> sprite_x{};
    if (player)
        for (int i = 0; i < 8; ++i) {
            sprite_x[i] = ram[0xac + i];
            cyc_mod_poke(0xac + i, uint8_t(ram[0x76 + i] * 256 + ram[0x91 + i] - local_camera));
        }
    cyc_mod_poke(0x12, local_camera >> 8);
    cyc_mod_poke(0xfd, local_camera & 255);
    projectile_world = world;
    updating_projectile = !player;
    updating_player_projectile = player;
    const bool done = cyc_mod_call_commit_hooked(player ? 0xa333 : 0xaf24, &registers);
    updating_projectile = updating_player_projectile = false;
    cyc_mod_poke(0x12, camera >> 8);
    cyc_mod_poke(0xfd, camera & 255);
    if (player)
        for (int i = 0; i < 8; ++i) cyc_mod_poke(0xac + i, sprite_x[i]);
    if (!done) return 0;
    const int next = unwrap(cyc_mod_peek(x_address), world);
    projectile_high(meta, uint8_t(next >> 8));
    projectile_tag(meta, cyc_mod_peek(id_address));
    for (int sprite = 0; sprite < 64; ++sprite) {
        const uint8_t *current = ram + 0x200 + sprite * 4;
        if (current[0] >= 192 || std::equal(current, current + 4, before.data() + sprite * 4)) continue;
        const int x = local_camera + unwrap(current[3], world - local_camera);
        const uint8_t screen_x = uint8_t(x - camera);
        extra_sprites[sprite] = {x, screen_x, current[0]};
        cyc_mod_poke(0x203 + sprite * 4, screen_x);
    }
    return 1;
}

inline int update_cannon(const uint8_t *ram, bool enabled) {
    if (!wide_level(ram, enabled) || cyc_mod_isolated()) return 0;
    CycModRegs registers{};
    cyc_mod_regs(&registers);
    const int slot = registers.x;
    if (slot >= 8 || !cyc_mod_peek(0x7a15 + slot)) return 0;
    const int world = cyc_mod_peek(0x7a2d + slot) * 256 + cyc_mod_peek(0x7a35 + slot);
    if (world < world_left(ram) - 32 || world >= world_left(ram) + cyc_video_width() + 32) return 1;
    const int camera = ram[0x12] * 256 + ram[0xfd], player = ram[0x75] * 256 + ram[0x90];
    const int local_camera = std::max(0, world - 128);
    const int proxy_player = std::abs(player - world) < 120 ? player : world + (player < world ? -120 : 120);
    std::array<uint8_t, 8> previous{};
    for (int i = 0; i < 8; ++i) previous[i] = cyc_mod_peek(SpecialId + i);
    cyc_mod_poke(0x12, local_camera >> 8);
    cyc_mod_poke(0xfd, local_camera & 255);
    cyc_mod_poke(0x75, proxy_player >> 8);
    cyc_mod_poke(0x90, proxy_player & 255);
    const bool done = cyc_mod_call_commit_hooked(0xbb81, &registers);
    cyc_mod_poke(0x12, camera >> 8);
    cyc_mod_poke(0xfd, camera & 255);
    cyc_mod_poke(0x75, player >> 8);
    cyc_mod_poke(0x90, player & 255);
    if (!done) return 0;
    for (int i = 0; i < 8; ++i)
        if (!previous[i] && cyc_mod_peek(SpecialId + i)) {
            projectile_high(i, 0x10 | (world >> 8));
            projectile_tag(i, world & 255);
        }
    return 1;
}
}
