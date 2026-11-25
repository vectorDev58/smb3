#pragma once
#include "pixels.h"

namespace smb3 {
using Sprite = std::array<uint8_t, 32 * 32>;

inline uint8_t character_role(uint32_t pixel, unsigned form, bool campaign) {
    if (!(pixel >> 24)) return 0;
    const uint32_t rgb = pixel & 0xffffff;
    if (!campaign) {
        if (rgb == 0xffd9b2) return 2;
        if (rgb == 0x1f1f1f || rgb == 0xf73910) return 3;
        return 1;
    }
    const int r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
    if (std::max({r, g, b}) < 120) return 3;
    if ((r > 160 && g > 80 && b > 45 && r * 100 > g * 112 && g > b) ||
        std::min({r, g, b}) > 150) return 2;
    if (form == 5) return 1;
    if (b * 100 > r * 115 && b > g) return g < 100 && b < 180 ? 3 : 1;
    if (r * 10 > g * 13 && b * 10 > g * 12) return r < 190 && b < 120 ? 3 : 1;
    return form == 6 ? 3 : 1;
}

inline Sprite sprite_roles(const Image &sheet, int left, int top, unsigned form, bool campaign) {
    Sprite result{};
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x)
            result[y * 32 + x] = character_role(sheet.at(left + x, top + y), form, campaign);
    return result;
}

inline Sprite campaign_sprite(const Image &sheet, unsigned form, unsigned frame) {
    if (sheet.width != 3072 || sheet.height != 224 || form > 6 || frame > 0x50) return {};
    return sprite_roles(sheet, frame * 32, form * 32, form, true);
}

enum class AirbornePose {
    None = -1, Jump, Fall, RunJump, Fly1, Fly2, Fly3, Flutter1, Flutter2, Flutter3,
    ThrowReady, ThrowRelease,
};

inline AirbornePose airborne_pose(unsigned form, unsigned frame, bool airborne, bool water, bool rising, bool attacking) {
    if (!airborne) return AirbornePose::None;
    // Fire and Hammer use the swim frames when throwing in mid-air, including underwater.
    if ((form == 2 || form == 6) && attacking && frame >= 0x34 && frame <= 0x36)
        return frame == 0x36 ? AirbornePose::ThrowReady : AirbornePose::ThrowRelease;
    if (water) return AirbornePose::None;
    if (form == 3 || form == 5) {
        // These frame numbers are shared with swimming and, for $09, the tail attack.
        if (frame >= 0x36 && frame <= 0x38)
            return static_cast<AirbornePose>(static_cast<int>(AirbornePose::Fly1) + frame - 0x36);
        if (frame >= 0x08 && frame <= 0x0a && !(frame == 0x09 && attacking))
            return static_cast<AirbornePose>(static_cast<int>(AirbornePose::Flutter1) + frame - 0x08);
        if (frame == 0x50) return rising ? AirbornePose::Jump : AirbornePose::Fall;
        return AirbornePose::None;
    }
    if (form == 4) return frame == 0x17 ? AirbornePose::Jump : AirbornePose::None;
    if (form == 0) {
        if (frame == 0x4e) return AirbornePose::RunJump;
        return frame == 0x40 ? AirbornePose::Jump : AirbornePose::None;
    }
    if (frame == 0x38) return AirbornePose::RunJump;
    if (frame == 0x4f) return AirbornePose::Jump;
    return frame == 0x06 ? AirbornePose::Fall : AirbornePose::None;
}

inline Sprite airborne_sprite(const Image &sheet, unsigned form, AirbornePose pose) {
    constexpr int width = (static_cast<int>(AirbornePose::ThrowRelease) + 1) * 32;
    if (sheet.width != width || sheet.height != 224 || form > 6 || pose == AirbornePose::None) return {};
    return sprite_roles(sheet, static_cast<int>(pose) * 32, form * 32, form, form >= 3);
}

inline Sprite reference_sprite(const Image &sheet, unsigned form, unsigned cell) {
    if (form > 6) return {};
    if (!form && cell >= 24 && cell < 28) return campaign_sprite(sheet, form, 0x1a + cell - 24);
    if (form >= 3) return campaign_sprite(sheet, form, cell);
    if (sheet.width != 256 || sheet.height != (form ? 256 : 192) || cell >= (form ? 32u : 24u)) return {};
    return sprite_roles(sheet, (cell % 8) * 32, (cell / 8) * 32, form, false);
}

inline bool sprite_visible(const Sprite &sprite) {
    return std::any_of(sprite.begin(), sprite.end(), [](uint8_t pixel) { return pixel != 0; });
}

inline unsigned title_luigi_frame(unsigned pose, bool falling) {
    // PRG0 gives the title actors their own indices for the same actions.
    switch (pose) {
        case 0x00: return falling ? 0x06 : 0x0c;
        case 0x01: return 0x0d;
        case 0x02: return 0x0e;
        case 0x06: return 0x0f;
        case 0x0a: return 0x32;
        case 0x0c: return 0x30;
        case 0x0d: return 0x41; // Foot extended to kick the shell.
        case 0x0e: return 0x2b;
        case 0x0f: return 0x29;
        case 0x10: return 0x2a;
        case 0x14: return 0x4f;
        default: return 0x51;
    }
}

inline unsigned reference_form(unsigned suit, unsigned frame) {
    if (suit == 0 || frame == 0x0b || frame == 0x4b) return 0;
    return frame == 0x27 ? 1 : std::min(suit, 6u);
}

inline unsigned campaign_frame(unsigned form, unsigned frame, bool facing_front) {
    // The ROM shares $05 with the tail attack. The atlas has the front view in $03.
    if (facing_front && frame == 0x05 && (form == 3 || form == 5)) return 0x03;
    return frame;
}

inline Sprite luigi_silhouette(const Sprite &source) {
    int left = 32, right = -1, top = 32, bottom = -1;
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x)
            if (source[y * 32 + x]) {
                left = std::min(left, x);
                right = std::max(right, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
    if (right < left) return source;
    const int width = right - left + 1, height = bottom - top + 1;
    const int new_width = width - (width >= 12 ? 2 : width >= 8 ? 1 : 0);
    const int new_top = std::max(0, top - (height >= 20 ? 2 : 1));
    const int new_height = bottom - new_top + 1;
    const int new_left = left + (width - new_width) / 2;
    Sprite result{};
    for (int y = 0; y < new_height; ++y)
        for (int x = 0; x < new_width; ++x) {
            const int src_y = top + (2 * y + 1) * height / (2 * new_height);
            const int src_x = left + (2 * x + 1) * width / (2 * new_width);
            result[(new_top + y) * 32 + new_left + x] = source[src_y * 32 + src_x];
        }
    return result;
}

inline bool player_poof(const uint8_t *sprites) {
    // A suit change draws two mirrored cloud tiles in the player's own OAM slots.
    const unsigned tile = sprites[1];
    return tile >= 0x41 && tile <= 0x47 && (tile & 1) && sprites[5] == tile &&
           sprites[0] == sprites[4] && sprites[3] == uint8_t(sprites[7] + 8) &&
           (sprites[2] & 0xc3) == 0xc1 && (sprites[6] & 0xc3) == 0x01;
}

inline int map_pose(unsigned power, uint8_t tile, uint8_t attributes, bool twirling, bool second_frame) {
    if (twirling) {
        switch (tile) {
        case 0x2d: return 2; // Front
        case 0x0f: return 3; // Side
        case 0x3d: return 4; // Back
        case 0x0d: return 5; // Other side
        default: return -1;
        }
    }
    constexpr uint8_t first[] = {0x2d, 0xb3, 0xb3, 0xb3, 0xd1, 0xc1, 0xcd, 0x9f, 0x8d};
    constexpr uint8_t second[] = {0x2f, 0xb7, 0xb7, 0xb7, 0xd3, 0xc3, 0xcf, 0x9f, 0xa7};
    if (power >= sizeof first || (attributes & 0x80)) return -1;
    if (power == 7 && tile == first[power]) return second_frame ? 1 : 0;
    if (tile == first[power]) return 0;
    return tile == second[power] ? 1 : -1;
}

inline bool campaign_pose(unsigned form, unsigned frame, bool swimming) {
    if (form >= 3 || frame == 0x43 || frame == 0x18) return true;
    if (form == 0)
        return frame == 0x2e || frame == 0x2f || frame == 0x42 || (frame >= 0x4c && frame <= 0x4e);
    return (frame >= 0x29 && frame <= 0x2d && frame != 0x2c) ||
           (frame >= 0x38 && frame <= 0x3b) || (form == 2 && !swimming && frame >= 0x34 && frame <= 0x36);
}

inline unsigned reference_cell(unsigned form, unsigned frame, bool moving, bool water) {
    if (form >= 3) return frame <= 0x50 ? frame : 0;
    if (!form) {
        switch (frame) {
        case 0x0b: return 0;
        case 0x07: case 0x0f: case 0x33: return 1;
        case 0x2e: case 0x42: return 2;
        case 0x3f: return 0;
        case 0x32: case 0x43: return 17;
        case 0x3e: return moving ? 2 : 0;
        case 0x40: case 0x4e: return 6;
        case 0x41: return 5;
        case 0x46: return water ? 7 : 9;
        case 0x47: return 10;
        case 0x48: return 11;
        case 0x49: return 8;
        case 0x4a: return 13;
        case 0x4b: return 15;
        case 0x4c: return 2;
        case 0x4d: return 0;
        case 0x1a: case 0x1b: case 0x1c: case 0x1d: return 24 + frame - 0x1a;
        default: return 0;
        }
    }
    switch (frame) {
    case 0x00: case 0x0c: return moving ? 2 : 0;
    case 0x01: case 0x0d: return moving ? 3 : 0;
    case 0x02: case 0x0e: return moving ? 4 : 0;
    case 0x2a: case 0x3a: return 3;
    case 0x06: case 0x2b: case 0x2d: case 0x3b: return 4;
    case 0x04: return 18;
    case 0x05: return 20;
    case 0x07: case 0x0f: case 0x18: case 0x31: return 1;
    case 0x08: case 0x09: case 0x0a: case 0x38: case 0x4f: case 0x50: return 6;
    case 0x10: case 0x11: return 15;
    case 0x1a: case 0x1b: case 0x1c: case 0x1d: return 24 + frame - 0x1a;
    case 0x27: return 16;
    case 0x28: return 13;
    case 0x2c: return 14;
    case 0x29: case 0x39: return 2;
    case 0x30: return 5;
    case 0x34: return water ? 10 : 12;
    case 0x35: return water ? 11 : 12;
    case 0x36: return water ? 12 : 6;
    case 0x37: return 6;
    case 0x3c: return 7;
    case 0x3d: return 8;
    default: return 0;
    }
}

}
