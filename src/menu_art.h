#pragma once
#include "pixels.h"
#include <cstdlib>

namespace smb3 {
constexpr uint32_t StageInk = 0xff221b1e;
constexpr uint32_t StageIvory = 0xfff8e9ce;
constexpr int StagePatternPeriod = 32;
constexpr int CurtainFoldPeriod = 64;

inline bool stage_repeating_row(const uint32_t *row) {
    // Ignore the outermost tile on either side, where the PPU can clip the picture.
    return std::equal(row + 8, row + 248 - StagePatternPeriod, row + 8 + StagePatternPeriod);
}

inline uint32_t stage_fabric(uint32_t pixel, int column) {
    const int fold = std::abs(wrap(column + CurtainFoldPeriod / 2, CurtainFoldPeriod) - CurtainFoldPeriod / 2);
    if (pixel == color(0x0f)) pixel = StageInk;
    return dim(pixel, 255 - fold / 4 * 5);
}

inline uint32_t stage_backdrop(uint32_t pixel, int x, int y, int width, int height) {
    const int column = (2 * x + 1 - width) * 32 / width;
    const int row = y * 32 / height - 18;
    const unsigned shade = (column * column + row * row) / 84 * 4;
    if (pixel == color(0x0f)) return blend(StageInk, (shade << 24) | 0x08060a);
    return blend(pixel, (shade << 24) | 0xb17046);
}

inline std::array<uint8_t, 80> menu_glyph(const std::vector<uint8_t> &rom, unsigned character) {
    std::array<uint8_t, 80> result{};
    bool acute = false, tilde = false, dots = false;
    switch (character) {
        case 0xc1: case 0xe1: character = 'A'; acute = true; break;
        case 0xc9: case 0xe9: character = 'E'; acute = true; break;
        case 0xcd: case 0xed: character = 'I'; acute = true; break;
        case 0xd3: case 0xf3: character = 'O'; acute = true; break;
        case 0xda: case 0xfa: character = 'U'; acute = true; break;
        case 0xd1: case 0xf1: character = 'N'; tilde = true; break;
        case 0xdc: case 0xfc: character = 'U'; dots = true; break;
    }
    if (character >= 'a' && character <= 'z') character -= 'a' - 'A';
    const int pattern = character >= 'A' && character <= 'Z' ? 0x30 + character - 'A' : -1;
    if (pattern >= 0 && rom.size() == 393216) {
        const uint8_t *tile = rom.data() + 262144 + 94 * 1024 + pattern * 16;
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
                result[(y + 2) * 8 + x] = tile_pixel(tile, x, y) != 3;
    } else {
        std::array<uint8_t, 8> rows{};
        switch (character) {
            case '0': rows = {0x3c,0x66,0x66,0x66,0x66,0x3c}; break;
            case '1': rows = {0x18,0x38,0x18,0x18,0x18,0x3c}; break;
            case '2': rows = {0x3c,0x66,0x0c,0x18,0x30,0x7e}; break;
            case '3': rows = {0x7c,0x06,0x3c,0x06,0x66,0x3c}; break;
            case '4': rows = {0x0c,0x1c,0x2c,0x4c,0x7e,0x0c}; break;
            case '5': rows = {0x7e,0x60,0x7c,0x06,0x66,0x3c}; break;
            case '6': rows = {0x1c,0x30,0x7c,0x66,0x66,0x3c}; break;
            case '7': rows = {0x7e,0x06,0x0c,0x18,0x30,0x30}; break;
            case '8': rows = {0x3c,0x66,0x3c,0x66,0x66,0x3c}; break;
            case '9': rows = {0x3c,0x66,0x66,0x3e,0x0c,0x38}; break;
            case '.': case 0xb7: rows = {0,0,0,0,0,0,0x18,0x18}; break;
            case ',': rows = {0,0,0,0,0,0x18,0x18,0x10}; break;
            case ':': rows = {0,0x18,0x18,0,0,0x18,0x18,0}; break;
            case ';': rows = {0,0x18,0x18,0,0,0x18,0x18,0x10}; break;
            case '!': rows = {0x18,0x18,0x18,0x18,0x18,0,0x18,0}; break;
            case '?': rows = {0x3c,0x66,0x06,0x0c,0x18,0,0x18,0}; break;
            case '<': rows = {0x06,0x0c,0x18,0x30,0x18,0x0c,0x06,0}; break;
            case '>': rows = {0x60,0x30,0x18,0x0c,0x18,0x30,0x60,0}; break;
            case '-': rows = {0,0,0,0x7e,0,0,0,0}; break;
            case '+': rows = {0,0x18,0x18,0x7e,0x18,0x18,0,0}; break;
            case '/': rows = {0x02,0x06,0x0c,0x18,0x30,0x60,0x40,0}; break;
            case '\\': rows = {0x40,0x60,0x30,0x18,0x0c,0x06,0x02,0}; break;
            case '_': rows = {0,0,0,0,0,0,0,0x7e}; break;
            case '=': rows = {0,0,0x7e,0,0x7e,0,0,0}; break;
            case '(': rows = {0x0c,0x18,0x30,0x30,0x30,0x18,0x0c,0}; break;
            case ')': rows = {0x30,0x18,0x0c,0x0c,0x0c,0x18,0x30,0}; break;
            case '%': rows = {0x62,0x66,0x0c,0x18,0x30,0x66,0x46,0}; break;
        }
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) result[(y + 2) * 8 + x] = (rows[y] >> (7 - x)) & 1;
    }
    if (acute) { result[4] = 1; result[8 + 3] = 1; }
    if (tilde) { result[2] = result[3] = result[5] = result[8 + 1] = result[8 + 4] = 1; }
    if (dots) result[2] = result[5] = 1;
    return result;
}

struct MenuArt { Image title, scenery, curtain, cursor; };
inline MenuArt menu_art(const std::vector<uint8_t> &rom) {
    MenuArt result;
    if (rom.size() != 393216) return result;
    std::array<uint8_t, 1024> nametable{};
    nametable.fill(0x5c);
    constexpr size_t bank = 25 * 8192, table = bank + 0x5da;
    for (int entry = 8; entry < 34; ++entry) {
        const int address = rom[table + entry * 2] | rom[table + entry * 2 + 1] << 8;
        if (address < 0xc000 || address >= 0xe000) return {};
        size_t cursor = bank + address - 0xc000;
        for (int command = 0; command < 64 && cursor + 3 < bank + 8192 && rom[cursor]; ++command) {
            int target = rom[cursor] * 256 + rom[cursor + 1];
            const int flags = rom[cursor + 2], count = flags & 63;
            cursor += 3;
            const int bytes = flags & 64 ? 1 : count;
            if (!count || cursor + bytes >= bank + 8192) return {};
            for (int i = 0; i < count; ++i) {
                if (target >= 0x2000 && target < 0x2400)
                    nametable[target - 0x2000] = rom[cursor + (flags & 64 ? 0 : i)];
                target += flags & 128 ? 32 : 1;
            }
            cursor += bytes;
        }
    }
    const uint8_t palette[] = {0x37,0x0f,0x30,0x16,0x37,0x0f,0x21,0x02,
                               0x37,0x0f,0x2a,0x1a,0x37,0x0f,0x27,0x02};
    result.title = {176, 120, std::vector<uint32_t>(176 * 120)};
    result.scenery = {256, 240, std::vector<uint32_t>(256 * 240)};
    result.curtain = {CurtainFoldPeriod, 24, std::vector<uint32_t>(CurtainFoldPeriod * 24)};
    for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 256; ++x) {
            const int tx = x / 8, ty = y / 8, pattern = nametable[ty * 32 + tx];
            const int attributes = nametable[960 + ty / 4 * 8 + tx / 4];
            const int group = (attributes >> ((ty % 4 / 2) * 4 + tx % 4 / 2 * 2)) & 3;
            const unsigned value = tile_pixel(rom.data() + 262144 + 120 * 1024 + pattern * 16, x & 7, y & 7);
            const uint32_t pixel = value ? color(palette[group * 4 + value]) : 0;
            const bool title = (ty >= 4 && ty <= 7 && tx >= 11 && tx <= 21) ||
                (ty >= 8 && ty <= 12 && tx >= 5 && tx <= 26) || (ty >= 13 && ty <= 18 && tx >= 14 && tx <= 20);
            if (title && x >= 40 && x < 216 && y >= 32 && y < 152)
                result.title.pixels[(y - 32) * 176 + x - 40] = pixel;
            if (group == 2) result.scenery.pixels[y * 256 + x] = pixel;
            if (x < result.curtain.width && y < result.curtain.height)
                result.curtain.pixels[y * result.curtain.width + x] = stage_fabric(color(palette[group * 4 + value]), x);
        }
    result.cursor = {8, 16, std::vector<uint32_t>(8 * 16)};
    const uint8_t cursor_palette[] = {0, 0x16, 0x36, 0x0f};
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 8; ++x) {
            const uint8_t *tile = rom.data() + 262144 + 127 * 1024 + 0x1e0 + (y / 8) * 16;
            const unsigned value = tile_pixel(tile, x, y & 7);
            if (value) result.cursor.pixels[y * 8 + x] = color(cursor_palette[value]);
        }
    return result;
}
}
