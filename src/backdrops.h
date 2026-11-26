#pragma once
#include "pixels.h"

namespace smb3 {
enum class Atmosphere { Clear, Wind, Bubbles, Snow, Embers };
enum class Scenery { Grass, Underground, Castle, Jungle, Snow, Night, Volcano, Bonus };
enum class Landform { None, Hills, Coast, Forest, Dunes, Peaks, Cliffs, Reef, Cloudbank };

struct Backdrop {
    const char *sky;
    const char *hills;
    const char *trees;
    uint32_t base;
    uint32_t horizon;
    bool fallback_clouds;
    Atmosphere atmosphere;
    Scenery scenery;
    Landform landform = Landform::None;
    int sky_row = 196;
    unsigned sky_opacity = 142;
    unsigned hills_opacity = 116;
    int sky_frame_ticks = 0;
};

inline Backdrop level_backdrop(int world, int tileset, int layout, int music, unsigned background, unsigned palette) {
    const bool dark = (background & 15) >= 13;
    static constexpr Backdrop desert{"Skies/DesertSky.png", "Desert", "DesertTrees", 0xffe6c7af, 0xffc89b7b,
                                    false, Atmosphere::Wind, Scenery::Grass, Landform::Dunes, 180, 142, 116};
    static constexpr Backdrop snow{"Skies/SnowSky.png", "Snow", "SnowTrees", 0xffd5e8ed, 0xffa3bacb,
                                  true, Atmosphere::Snow, Scenery::Snow, Landform::Peaks, 202, 108, 142};
    static constexpr Backdrop cave{nullptr, "Underground", nullptr, 0xff101922, 0xff293848,
                                  false, Atmosphere::Clear, Scenery::Underground, Landform::Cliffs, 0, 0, 82};
    static constexpr Backdrop war{"Skies/MountainStars.png", "Volcano", nullptr, 0xff151b25, 0xff473b49,
                                 false, Atmosphere::Embers, Scenery::Volcano, Landform::Peaks, 0, 137, 66, 12};
    static constexpr Backdrop night_sky{"Skies/MountainStars.png", nullptr, nullptr, 0xff182638, 0xff182638,
                                       false, Atmosphere::Clear, Scenery::Night, Landform::None, 158, 160, 0, 12};
    static constexpr Backdrop outdoors[] = {
        {"Skies/DaySky.png", "Overworld", nullptr, 0xffa8e3da, 0xff63b88c,
            true, Atmosphere::Clear, Scenery::Grass, Landform::Hills},
        desert,
        {"Skies/DaySky.png", "BeachHills", nullptr, 0xffb5e1df, 0xff81b6b0,
            true, Atmosphere::Clear, Scenery::Grass, Landform::Coast},
        {"Skies/DaySky.png", "GardenHill", nullptr, 0xffb8dfca, 0xff80ae7f,
            true, Atmosphere::Clear, Scenery::Jungle, Landform::Forest},
        {"Skies/DaySky.png", "Mountain", nullptr, 0xffb5dce0, 0xff8ba7b5,
            true, Atmosphere::Clear, Scenery::Grass, Landform::Peaks},
        snow,
        {"Skies/DaySky.png", "Overworld", "PipelandTrees", 0xffb3e1d4, 0xff7bba99,
            true, Atmosphere::Clear, Scenery::Jungle, Landform::Forest},
        war,
    };
    if (tileset == 6 || layout == 6 || music == 0x30)
        return {"Hills/CastleWater.png", "Underwater", nullptr, 0xff3a5488, 0xff243b63,
                false, Atmosphere::Bubbles, Scenery::Underground, Landform::Reef, 320, 26, 80, 30};
    if (tileset == 2 || layout == 2)
        return {nullptr, "Castle", nullptr, 0xff0a1117, 0xff222f39,
                false, Atmosphere::Clear, Scenery::Castle, Landform::Cliffs, 0, 0, 176};
    if (tileset == 8 || tileset == 14 || music == 0x20 || layout == 8 || layout == 14)
        return cave;
    if (layout == 15 || layout == 22 || tileset == 7)
        return {nullptr, nullptr, nullptr, 0xff30251d, 0xff30251d, false, Atmosphere::Clear, Scenery::Bonus};
    if (layout == 21) return war;
    if (tileset == 10) {
        if (palette == 5 || music == 0x50)
            return {nullptr, nullptr, nullptr, 0xff151c24, 0xff151c24, false, Atmosphere::Clear, Scenery::Night};
        if (dark) return night_sky;
        return {"Hills/AirshipHills.png", nullptr, nullptr, 0xffacdce0, 0xffe5f5ee,
                true, Atmosphere::Clear, Scenery::Night, Landform::Cloudbank, 135, 183};
    }
    if (tileset == 4 || tileset == 13 || layout == 4 || layout == 13 || music == 0x09) {
        if (dark) return night_sky;
        return {"Skies/MountainSky.png", nullptr, nullptr, 0xffafdfdf, 0xffe8f6f1,
                true, Atmosphere::Clear, Scenery::Grass, Landform::Cloudbank, 196, 170};
    }
    if (tileset == 12 || layout == 12) {
        if (dark)
            return {"Skies/SnowStars.png", "SnowNight", "SnowNightTrees", 0xff162734, 0xff293f56,
                    false, Atmosphere::Snow, Scenery::Snow, Landform::Peaks, 148, 126, 84, 12};
        return snow;
    }
    if (dark) return world == 7 ? war : cave;
    if (tileset == 9 || layout == 9) return desert;
    return outdoors[std::clamp(world, 0, 7)];
}

struct BackdropFrame { int width, left; };

inline BackdropFrame backdrop_frame(const Image &layer, int tick, int frame_ticks) {
    if (!frame_ticks) return {layer.width, 0};
    // The two 512-pixel frames in SMB3-mastered v0.9 are side by side.
    constexpr int FrameSize = 512;
    if (layer.width != FrameSize * 2 || layer.height != FrameSize) return {0, 0};
    return {FrameSize, (tick / frame_ticks % 2) * FrameSize};
}

inline Image landscape_layer(const Backdrop &style, const uint8_t *hill_pattern, bool distant) {
    if (style.landform == Landform::None) return {};
    constexpr int Width = 512, Height = 112;
    struct Ridge { int center, half_width, height; };
    static constexpr Ridge near[] = {{13, 24, 88}, {46, 28, 66}, {97, 23, 38}, {171, 29, 78},
                                    {209, 24, 61}, {291, 35, 42}, {372, 29, 74}, {411, 25, 50}};
    static constexpr Ridge far[] = {{13, 37, 38}, {48, 28, 55}, {132, 36, 29}, {185, 28, 48},
                                   {246, 39, 76}, {276, 33, 51}, {350, 42, 40}, {468, 48, 63}};
    const bool coast = style.landform == Landform::Coast;
    const bool tall = style.landform == Landform::Forest || style.landform == Landform::Reef;
    const bool cliffs = style.landform == Landform::Cliffs;
    const bool dune = style.landform == Landform::Dunes;
    const bool peak = style.landform == Landform::Peaks || dune;
    const bool clouds = style.landform == Landform::Cloudbank;
    const bool tile_texture = hill_pattern && !distant && (style.landform == Landform::Hills || style.landform == Landform::Forest);
    const uint32_t body = blend(style.base, (style.horizon & 0xffffff) | (distant ? 0x68000000u : 0xc4000000u));
    const uint32_t light = blend(body, distant ? 0x12fff9eb : 0x30fff9eb);
    const uint32_t dark = blend(body, 0x20344851);
    Image result{Width, Height, std::vector<uint32_t>(Width * Height)};
    for (const auto &ridge : distant ? far : near) {
        const int half_width = tall ? ridge.half_width * 2 / 3 :
            dune || clouds || coast ? ridge.half_width * 3 / 2 : ridge.half_width;
        const int height = tall ? std::min(Height, ridge.height * 5 / 4) : coast ? ridge.height * 3 / 5 : ridge.height;
        for (int dx = -half_width; dx < half_width; ++dx) {
            const int x = wrap(ridge.center + dx, Width);
            const int distance = dx < 0 ? -dx : dx;
            const int radius = dune && dx > 0 ? half_width * 2 / 3 : half_width;
            int top = Height - height;
            if (peak) top += height * distance / radius;
            else if (cliffs) top += distance / 4 * 2;
            else top += std::min(height, half_width) * dx * dx / (half_width * half_width);
            for (int y = top; y < Height; ++y) {
                uint32_t pixel = body;
                if (tile_texture) pixel = tile_pixel(hill_pattern, x & 7, y & 7) == 2 ? light : body;
                else if (peak && dx > 0) pixel = dark;
                else if (cliffs && ((x + y / 8) & 7) == 0) pixel = dark;
                if (y - top < 2 && (!peak || dx <= 0)) pixel = light;
                result.pixels[y * Width + x] = pixel;
            }
        }
    }
    return result;
}

enum class WaterTile { Dry, Fill, Wave, SeaSurface, Submerged };

inline WaterTile water_tile(int tileset, unsigned tile, const uint8_t *limits, const uint8_t *solid) {
    const unsigned quadrant = tile >> 6;
    if (tile <= limits[quadrant] || tile >= solid[quadrant]) return WaterTile::Dry;
    if ((tileset == 6 || tileset == 8) && tile == 0x8b) return WaterTile::SeaSurface;
    const bool hills = tileset == 3 || tileset == 14;
    if ((hills && tile == 0xc2) || (tile >= 0xdb && tile <= 0xdd) || tile == 0xe0 || tile == 0xe1)
        return WaterTile::Wave;
    if (((tileset == 6 || tileset == 8) && tile == 0x8c) || (hills && tile == 0xc3) || tile == 0xde)
        return WaterTile::Fill;
    return WaterTile::Submerged;
}

inline uint32_t water_overlay(WaterTile tile, unsigned texel, int row, uint32_t body, uint32_t crest) {
    if (tile == WaterTile::Dry || (tile == WaterTile::SeaSurface && row < 4)) return 0;
    if (tile == WaterTile::Wave) {
        if (!texel) return 0;
        return texel == 3 ? body : crest;
    }
    return body;
}

inline bool water_partition(unsigned update, unsigned raster, bool solid_floor) {
    return update == 0xa0 && (raster == 0xa0 || (raster == 0x40 && !solid_floor));
}

inline int atlas_row(Scenery scenery, bool coin) {
    switch (scenery) {
    case Scenery::Underground: return 1;
    case Scenery::Castle: return 2;
    case Scenery::Jungle: return coin ? 0 : 3;
    case Scenery::Snow: return coin ? 3 : 0;
    case Scenery::Night: return coin ? 4 : 1;
    case Scenery::Volcano: return coin ? 5 : 1;
    case Scenery::Bonus: return coin ? 6 : 0;
    default: return 0;
    }
}

enum class TileAnimation { None, Question, Brick, Coin };
inline TileAnimation tile_animation(unsigned tile) {
    if (tile >= 0x60 && tile <= 0x65) return TileAnimation::Question;
    if (tile >= 0x67 && tile <= 0x70) return TileAnimation::Brick;
    if (tile == 0x40) return TileAnimation::Coin;
    return TileAnimation::None;
}

inline uint32_t animated_tile_color(TileAnimation animation, unsigned palette, unsigned texel, int x, int y, int frame) {
    const uint32_t base = color(palette);
    if (animation == TileAnimation::None || (palette != 0x27 && palette != 0x36)) return base;
    if (animation == TileAnimation::Question) {
        // Keep the ROM's question mark and outline beneath a warm face and moving highlight.
        if (texel == 3) {
            const int sheen = x + y - (7 + frame * 5);
            return sheen == 0 || sheen == 1 ? 0xffffbf7b : 0xfff3a955;
        }
        if (y < 3) return 0xffffebc8;
    }
    if (animation == TileAnimation::Brick && (y & 7) == 1) return blend(base, 0x42fff5dc);
    if (animation == TileAnimation::Coin && texel == 2 && x == 5 + frame) return blend(base, 0x73fff5dc);
    return base;
}
}
