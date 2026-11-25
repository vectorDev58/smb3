#pragma once
#include "pixels.h"

namespace smb3 {
enum class Atmosphere { Clear, Wind, Bubbles, Snow, Embers };
enum class Scenery { Grass, Underground, Castle, Jungle, Snow, Night, Volcano, Bonus };

struct Backdrop {
    const char *sky;
    const char *hills;
    const char *trees;
    uint32_t base;
    uint32_t horizon;
    bool fallback_clouds;
    Atmosphere atmosphere;
    Scenery scenery;
    int sky_row = 196;
    unsigned sky_opacity = 142;
    unsigned hills_opacity = 116;
    int sky_frame_ticks = 0;
};

inline Backdrop level_backdrop(int world, int tileset, int layout, int music, unsigned background, unsigned palette) {
    const bool dark = (background & 15) >= 13;
    static constexpr Backdrop desert{"Skies/DesertSky.png", "Desert", "DesertTrees", 0xffe6c7af, 0xffc89b7b,
                                    false, Atmosphere::Wind, Scenery::Grass, 180, 142, 116};
    static constexpr Backdrop snow{"Skies/SnowSky.png", "Snow", "SnowTrees", 0xffd5e8ed, 0xffa3bacb,
                                  true, Atmosphere::Snow, Scenery::Snow, 202, 108, 142};
    static constexpr Backdrop cave{nullptr, "Underground", nullptr, 0xff101922, 0xff101922,
                                  false, Atmosphere::Clear, Scenery::Underground, 0, 0, 82};
    static constexpr Backdrop war{"Skies/MountainStars.png", "Volcano", nullptr, 0xff151b25, 0xff24232d,
                                 false, Atmosphere::Embers, Scenery::Volcano, 0, 137, 66, 12};
    static constexpr Backdrop night_sky{"Skies/MountainStars.png", nullptr, nullptr, 0xff182638, 0xff182638,
                                       false, Atmosphere::Clear, Scenery::Night, 158, 160, 0, 12};
    static constexpr Backdrop outdoors[] = {
        {"Skies/DaySky.png", "Overworld", nullptr, 0xfface1d8, 0xff80b995, true, Atmosphere::Clear, Scenery::Grass},
        desert,
        {"Skies/DaySky.png", "BeachHills", nullptr, 0xffb5e1df, 0xff9cbcaa, true, Atmosphere::Clear, Scenery::Grass},
        {"Skies/DaySky.png", "GardenHill", nullptr, 0xffb8dfca, 0xff92b688, true, Atmosphere::Clear, Scenery::Jungle},
        {"Skies/DaySky.png", "Mountain", nullptr, 0xffb5dce0, 0xff9ab6a4, true, Atmosphere::Clear, Scenery::Grass},
        snow,
        {"Skies/DaySky.png", "Overworld", "PipelandTrees", 0xffb3e1d4, 0xff8db99b, true, Atmosphere::Clear, Scenery::Jungle},
        war,
    };
    if (tileset == 6 || layout == 6 || music == 0x30)
        return {"Hills/CastleWater.png", "Underwater", nullptr, 0xff3a5488, 0xff3a5488,
                false, Atmosphere::Bubbles, Scenery::Underground, 320, 26, 80, 30};
    if (tileset == 2 || layout == 2)
        return {nullptr, "Castle", nullptr, 0xff0a1117, 0xff0a1117, false, Atmosphere::Clear, Scenery::Castle, 0, 0, 176};
    if (tileset == 8 || tileset == 14 || music == 0x20 || layout == 8 || layout == 14)
        return cave;
    if (layout == 15 || layout == 22 || tileset == 7)
        return {nullptr, nullptr, nullptr, 0xff30251d, 0xff30251d, false, Atmosphere::Clear, Scenery::Bonus};
    if (layout == 21) return war;
    if (tileset == 10) {
        if (palette == 5 || music == 0x50)
            return {nullptr, nullptr, nullptr, 0xff151c24, 0xff151c24, false, Atmosphere::Clear, Scenery::Night};
        if (dark) return night_sky;
        return {"Hills/AirshipHills.png", nullptr, nullptr, 0xffacdce0, 0xffacdce0,
                true, Atmosphere::Clear, Scenery::Night, 135, 183};
    }
    if (tileset == 4 || tileset == 13 || layout == 4 || layout == 13 || music == 0x09) {
        if (dark) return night_sky;
        return {"Skies/MountainSky.png", nullptr, nullptr, 0xffafdfdf, 0xffafdfdf,
                true, Atmosphere::Clear, Scenery::Grass, 196, 170};
    }
    if (tileset == 12 || layout == 12) {
        if (dark)
            return {"Skies/SnowStars.png", "SnowNight", "SnowNightTrees", 0xff162734, 0xff293f56,
                    false, Atmosphere::Snow, Scenery::Snow, 148, 126, 84, 12};
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
}
