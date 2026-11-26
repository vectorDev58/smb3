#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace smb3 {
struct Image {
    int width = 0, height = 0;
    std::vector<uint32_t> pixels;
    bool empty() const { return pixels.empty(); }
    uint32_t at(int x, int y) const {
        return x >= 0 && y >= 0 && x < width && y < height ? pixels[y * width + x] : 0;
    }
};

inline unsigned tile_pixel(const uint8_t *tile, int x, int y) {
    const int shift = 7 - x;
    return ((tile[y] >> shift) & 1) | (((tile[y + 8] >> shift) & 1) << 1);
}

inline uint32_t blend(uint32_t under, uint32_t over) {
    const unsigned alpha = over >> 24;
    uint32_t result = 0xff000000;
    for (int shift : {0, 8, 16}) {
        unsigned value = (((over >> shift) & 255) * alpha +
                          ((under >> shift) & 255) * (255 - alpha) + 127) / 255;
        result |= value << shift;
    }
    return result;
}

inline uint32_t dim(uint32_t pixel, unsigned light) {
    if (light == 255) return pixel;
    uint32_t result = pixel & 0xff000000;
    for (int shift : {0, 8, 16})
        result |= ((((pixel >> shift) & 255) * light + 127) / 255) << shift;
    return result;
}

// Classic graphics point this at the console's own 64 colors for the frame; null keeps the remaster's.
inline const uint32_t *classic_palette = nullptr;

// Shared NES color indices keep every original animation, tile and palette cycle.
inline uint32_t color(unsigned index) {
    if (classic_palette) return classic_palette[index & 63];
    static constexpr uint32_t palette[64] = {
        0x77756c,0x313c85,0x373298,0x5f2c87,0x842963,0x963a43,0x9d492b,0x85512b,
        0x606326,0x397535,0x287644,0x287368,0x2b6375,0x292823,0x292823,0x292823,
        0xb6b3a4,0x5284ce,0x6666d7,0x9a5ac9,0xc3579c,0xe26970,0xe68644,0xd49648,
        0xaab448,0x73bf52,0x48b970,0x40b59b,0x42a9bb,0x292823,0x292823,0x292823,
        0xfff5dc,0x9bc8ee,0xaaa5f0,0xd698e6,0xf393cd,0xffa5b3,0xffbf7b,0xf5d287,
        0xdde38e,0xb4e987,0x94e4a4,0x8df3df,0xa8e9e4,0x77756c,0x292823,0x292823,
        0xfff9eb,0xd5e9fa,0xdedcfa,0xedd6f5,0xfad5ee,0xffdcd8,0xffe4c5,0xffedca,
        0xf1f2cd,0xe0f5c9,0xd1f2d6,0xcbf7e8,0xd5f3ed,0xb6b3a4,0x292823,0x292823
    };
    return 0xff000000 | palette[index & 63];
}

inline uint32_t sprite_color(unsigned index) {
    index &= 63;
    if (classic_palette) return classic_palette[index];
    if ((index & 15) >= 14 || index == 0x0d || index == 0x1d) return 0xff1f1f1f;
    switch (index) {
    case 0x16: return 0xfff73910;
    case 0x27: return 0xffffaf55;
    case 0x2a: return 0xff28b835;
    case 0x36: return 0xffffd9b2;
    default: return color(index);
    }
}

inline unsigned sprite_palette_light(const uint8_t *current, const uint8_t *target) {
    auto brightest = [](const uint8_t *palette) {
        unsigned brightness = 0;
        for (int i = 1; i < 4; ++i) {
            const unsigned shade = (palette[i] >> 4) & 3;
            const unsigned hue = palette[i] & 15;
            if (hue < 13 || (hue == 13 && shade >= 2))
                brightness = std::max(brightness, shade + 1);
        }
        return brightness;
    };
    const unsigned full = brightest(target);
    return full ? std::min(brightest(current), full) * 255 / full : 255;
}

inline int wrap(int value, int size) { return (value % size + size) % size; }

inline int unwrap(int value, int reference, int period = 256) {
    return reference + wrap(value - reference + period / 2, period) - period / 2;
}

// Scores, debris, coins and the shell's kill flash keep an eight-bit X relative to the NES screen.
// Pick the copy, 256 pixels apart, nearest to whatever spawned it (the player, an actor or an effect).
inline int wrapped_effect_x(int x8, const std::vector<int> &anchors, int left, int right) {
    int best = x8, distance = 1 << 30;
    for (int x = x8 - 768; x < x8 + 1024; x += 256) {
        if (x < left || x >= right) continue;
        for (int anchor : anchors)
            if (std::abs(x - anchor) < distance) { distance = std::abs(x - anchor); best = x; }
    }
    return best;
}

// SMB1 Remastered's rainbow star: the sprite's shading over a hue that moves with phase.
inline uint32_t rainbow(uint32_t pixel, int phase) {
    const unsigned light = (((pixel >> 16) & 255) * 77 + ((pixel >> 8) & 255) * 151 + (pixel & 255) * 28) >> 8;
    if (light < 48) return pixel; // Outlines stay dark.
    const int hue = wrap(phase, 360);
    auto channel = [&](int n) {
        const int k = (n * 60 + hue) % 360;
        const int ramp = std::max(0, std::min({k, 240 - k, 60}));
        return unsigned(255 - 255 * ramp / 60) * (96 + light * 5 / 8) / 255;
    };
    return 0xff000000 | channel(5) << 16 | channel(3) << 8 | channel(1);
}

inline int native_origin(int camera, int width, int level_width) {
    if (level_width <= width) return std::clamp((width - level_width) / 2 + camera, 0, width - 256);
    const int left = std::clamp(camera - (width - 256) / 2, 0, level_width - width);
    return std::clamp(camera - left, 0, width - 256);
}

inline int level_origin(int camera, int width, int level_width, int goal_x = -1) {
    if (goal_x < 0) return native_origin(camera, width, level_width);
    const int center = (width - 256) / 2;
    const int start = std::max(0, center - std::max(0, goal_x - 192));
    return std::clamp(camera + start, 0, center);
}

inline int goal_position(const uint8_t *cart, size_t size) {
    if (!cart || size < 0x1bd1) return -1;
    for (int index = 0; index < 48; ++index) {
        const int id = cart[0x1b41 + index * 3];
        if (id == 0xff) break;
        if (id == 0x41) return cart[0x1b42 + index * 3] * 16;
    }
    return -1;
}

inline int backdrop_height(int camera_y) { return camera_y - 239; }

inline int parallax_offset(int view_left, int width, int divisor) {
    return (view_left + width / 2) / divisor - width / 2;
}

inline uint32_t sprite_priority(uint32_t previous, uint32_t pixel, bool behind, bool opaque) {
    // A sprite behind the background still masks sprites with a lower OAM priority.
    return !(pixel >> 24) ? previous : behind && opaque ? 0 : pixel;
}

template<typename Pixel>
inline void shadow(uint32_t *canvas, int width, int height, const std::vector<Pixel> &mask,
                   int dx = 3, int dy = 4, const uint8_t *cover = nullptr) {
    for (int y = dy; y < height; ++y)
        for (int x = dx; x < width; ++x) {
            const int i = y * width + x;
            if (mask[(y - dy) * width + x - dx] && !mask[i] && (!cover || !cover[i]))
                canvas[i] = blend(canvas[i], 0x66000000);
        }
}
}
