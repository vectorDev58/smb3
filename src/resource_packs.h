#pragma once
#include "pixels.h"
#include <array>
#include <string>
#include <utility>
#include <vector>

// Art built at startup from the player's own downloads in resource_packs/.
// Each builder reproduces the earlier local importers pixel for pixel and returns
// nothing when a sheet is a different revision, so the game keeps its ROM poses.
namespace smb3 {
using NamedImages = std::vector<std::pair<std::string, Image>>;

inline Image blank_image(int width, int height) {
    Image image;
    image.width = width;
    image.height = height;
    image.pixels.assign(static_cast<size_t>(width) * height, 0);
    return image;
}

// Expanded SMM2 SMB3 sprites, sheet 143355 on The Spriters Resource (1961 x 4290).
constexpr int CampaignSheetWidth = 1961, CampaignSheetHeight = 4290, CampaignFrames = 96;

struct SheetPose { int x, row, height, padding; };

// Columns and rows of Mario's block. Luigi, Toad and Toadette share the layout.
inline std::array<SheetPose, CampaignFrames> campaign_poses(unsigned suit) {
    static constexpr int rows[] = {24, 212, 484, 751, 1114, 1423, 1786};
    const int main = rows[suit], height = suit ? 32 : 16;
    std::array<SheetPose, CampaignFrames> poses;
    poses.fill({1, main, height, 32 - height});
    auto assign = [&](const std::vector<int> &frames, const std::vector<int> &columns, int row = -1, int padding = -1) {
        for (size_t i = 0; i < frames.size(); ++i)
            poses[frames[i]] = {columns[i % columns.size()], row < 0 ? main : row, height,
                                padding < 0 ? 32 - height : padding};
    };
    if (suit == 0) {
        assign({0x3e, 0x3f}, {108, 141});
        assign({0x4c, 0x4d}, {178, 211});
        assign({0x40, 0x4e}, {285, 355});
        assign({0x30, 0x41}, {248});
        assign({0x31, 0x33}, {737});
        assign({0x07, 0x0f}, {38});
        assign({0x32, 83}, {458});
        assign({0x46, 0x47, 0x48}, {564, 597, 630});
        assign({0x49}, {527});
        assign({0x2c, 0x4a}, {667});
        assign({0x4b}, {774});
        assign({0x1a, 0x1b, 0x1c, 0x1d}, {1, 34, 67, 100}, 64);
        assign({92, 93, 94, 95}, {133, 166, 199, 232}, 64);
        assign({0x29, 0x2a, 0x2b, 0x2e, 0x2f}, {620, 653}, 64);
        assign({82}, {583}, 64);
        assign({90, 91}, {690, 723}, 64);
        assign({0x2d, 0x42}, {797}, 64);
        assign({0x43}, {1}, 182, 4);
    } else if (suit == 1 || suit == 2 || suit == 6) {
        assign({0, 1, 2, 0x0c, 0x0d, 0x0e}, {108, 141, 174});
        assign({0x39, 0x3a, 0x3b}, {211, 244, 277});
        assign({0x06, 0x4f, 0x50}, {351, 384});
        assign({0x38}, {421});
        assign({0x30, 0x41}, {314});
        assign({0x31, 0x33}, {suit != 6 ? 837 : 314});
        assign({0x07, 0x0f}, {38});
        assign({0x05, 0x32, 83}, {524});
        assign({0x34, 0x35, 0x36}, {664, 697, 730});
        assign({0x3c, 0x3d}, {561, 594});
        assign({0x2c, 0x4a}, {767});
        assign({0x4b}, {suit != 6 ? 874 : 837});
        // Star row, then the columns for idle carry, walking carry (3), carry jump (2) and kick.
        static constexpr int carry[3][8] = {{272, 615, 652, 685, 718, 755, 788, 825},
                                            {539, 412, 449, 482, 515, 552, 585, 622},
                                            {1841, 449, 486, 519, 552, 589, 622, 659}};
        const int *c = carry[suit == 1 ? 0 : suit == 2 ? 1 : 2];
        const int star = c[0], first = suit == 1 ? 1 : 71;
        assign({0x1a, 0x1b, 0x1c, 0x1d}, {first, first + 33, first + 66, first + 99}, star);
        assign({92, 93, 94, 95}, {first + 132, first + 165, first + 198, first + 231}, star);
        assign({0x29, 0x2a, 0x2b}, {c[2], c[3], c[4]}, star);
        assign({82}, {c[1]}, star);
        assign({90, 91}, {c[5], c[6]}, star);
        assign({0x2d}, {c[7]}, star);
        if (suit != 1) assign({0x10, 0x11, 87, 88, 89}, {1, 34}, star);
        assign({0x18}, {1}, suit == 1 ? 438 : suit == 2 ? 705 : 2007);
    } else if (suit == 3 || suit == 5) {
        const int second = suit == 3 ? 795 : 1467, carry = suit == 3 ? 849 : 1521;
        assign({0, 1, 2, 0x0c, 0x0d, 0x0e}, {211, 244, 277});
        assign({0x39, 0x3a, 0x3b}, {314, 347, 380});
        assign({0x06, 0x4f, 0x50}, {454, 487});
        assign({0x30, 0x41}, {417});
        assign({0x07, 0x0f}, {38});
        assign({0x08, 0x09, 0x0a}, {524, 557, 590});
        assign({84, 85, 86}, {697, 730, 763});
        assign({83}, {833});
        assign({0x03, 0x04, 0x05}, {351, 384, 417}, second);
        assign({0x31, 0x33}, {277}, second);
        assign({0x34, 0x35, 0x36}, {104, 137, 170}, second);
        assign({0x3c, 0x3d}, {1, 34}, second);
        assign({0x2c}, {207}, second);
        assign({0x4b}, {314}, second);
        assign({0x1a, 0x1b, 0x1c, 0x1d}, {487, 520, 553, 586}, second);
        assign({92, 93, 94, 95}, {619, 652, 685, 718}, second);
        if (suit == 5) assign({0x1e}, {829}, second);
        assign({0x29, 0x2a, 0x2b}, {306, 339, 372}, carry);
        assign({82}, {269}, carry);
        assign({90, 91}, {512, 545}, carry);
        assign({0x2d}, {858}, carry);
        assign({0x18}, {1}, suit == 3 ? 1068 : 1740);
    } else {
        assign({0, 1, 2, 0x15, 0x16, 0x17}, {108, 141, 174});
        assign({0x07, 0x0f}, {38});
        assign({0x30, 0x41}, {248});
        assign({0x06, 0x4f, 0x50, 0x38}, {281, 314});
        assign({0x05, 0x32, 83}, {384});
        assign({0x1f, 0x20}, {421, 454});
        assign({0x12, 0x13, 0x14}, {491, 524, 557});
        assign({0x21, 0x22, 0x23}, {594, 627, 660});
        assign({0x24, 0x25, 0x26}, {697, 730, 763});
        assign({0x28, 0x2c}, {800});
        assign({0x31, 0x33}, {1}, 1158);
        assign({0x4b}, {71}, 1158);
        assign({0x1a, 0x1b, 0x1c, 0x1d}, {108, 141, 174, 207}, 1158);
        assign({92, 93, 94, 95}, {240, 273, 306, 339}, 1158);
        assign({0x29, 0x2a, 0x2b}, {38, 71, 104}, 1212);
        assign({82}, {1}, 1212);
        assign({90, 91}, {211, 244}, 1212);
        assign({0x2d}, {487}, 1212);
        assign({0x18}, {1}, 1377);
    }
    poses[0x0b] = {1, 24, 16, 16};
    poses[0x27] = {1, 212, 32, 0};
    return poses;
}

// Moves this sheet revision's colors to the palette of the other character art.
inline uint32_t campaign_color(uint32_t rgb, int character) {
    if (character == 2 && rgb == 0x0060c8) return 0x007eff;
    if (character == 3) {
        if (rgb == 0xf04060) return 0xff50c8;
        if (rgb == 0xc82838) return 0xc93497;
        if (rgb == 0xa00000) return 0x911e6f;
    }
    switch (rgb) {
    case 0x282828: return 0x1f1f1f;
    case 0xf8b880: return 0xffd9b2;
    case 0xf09058: return 0xe8ad7e;
    case 0xf8f8f8: return 0xfff8ea;
    default: return rgb;
    }
}

// Players/<character>/Campaign.png: 96 ROM frame numbers by seven suits, 32 x 32 each.
inline NamedImages campaign_art(const Image &sheet) {
    if (sheet.width != CampaignSheetWidth || sheet.height != CampaignSheetHeight) return {};
    static const char *const names[] = {"Luigi", "Toad", "Toadette"};
    NamedImages result;
    for (int character = 1; character <= 3; ++character) {
        const int left = character == 2 ? 0 : 980, top = character == 1 ? 0 : 2041;
        Image atlas = blank_image(CampaignFrames * 32, 7 * 32);
        for (unsigned suit = 0; suit < 7; ++suit) {
            const auto poses = campaign_poses(suit);
            for (int frame = 0; frame < CampaignFrames; ++frame) {
                const SheetPose &pose = poses[frame];
                // Frame $27 is scaled from 32 to 24 rows. Pillow's nearest-neighbour resize
                // accumulates the source row in doubles, which decides four of the rows.
                const bool shrink = frame == 0x27;
                const int rows = shrink ? 24 : pose.height, padding = shrink ? 8 : pose.padding;
                bool visible = false;
                double scaled_row = 32.0 / 24.0 * 0.5;
                for (int y = 0; y < rows; ++y, scaled_row += 32.0 / 24.0) {
                    const int source_y = shrink ? static_cast<int>(scaled_row) : y;
                    for (int x = 0; x < 32; ++x) {
                        const uint32_t pixel = sheet.at(left + pose.x + x, top + pose.row + source_y);
                        const uint32_t rgb = pixel & 0xffffff;
                        const bool backdrop = rgb == 0x2c3b6d || rgb == 0x849dbd || rgb == 0x3a6f33 || rgb == 0xffaaa9;
                        const uint32_t alpha = backdrop ? 0 : pixel & 0xff000000;
                        visible |= alpha != 0;
                        atlas.pixels[(suit * 32 + padding + y) * atlas.width + frame * 32 + x] =
                            alpha | campaign_color(rgb, character);
                    }
                }
                if (!visible) return {};
            }
        }
        result.emplace_back(std::string("Players/") + names[character - 1] + "/Campaign.png", std::move(atlas));
    }
    return result;
}

// Newer revision of the same sheet, published by GlacialSiren484 on Newgrounds (1961 x 4246).
constexpr int ReferenceSheetWidth = 1961, ReferenceSheetHeight = 4246;

inline bool cell_background(uint32_t pixel) {
    const uint32_t rgb = pixel & 0xffffff;
    return rgb == 0x91a0cc || rgb == 0x3a6f33;
}

// Copies the 32-pixel-wide cell from rows [top, bottom) and keys out the sheet backgrounds.
inline bool copy_cell(Image &target, int left, int top_row, const Image &sheet, int x, int top, int bottom) {
    bool visible = false;
    for (int y = top; y < bottom; ++y)
        for (int dx = 0; dx < 32; ++dx) {
            const uint32_t pixel = sheet.at(x + dx, y), rgb = pixel & 0xffffff;
            const bool backdrop = cell_background(pixel) || rgb == 0x2f3368;
            visible |= !backdrop;
            target.pixels[(top_row + y - top) * target.width + left + dx] = (backdrop ? 0 : 0xff000000u) | rgb;
        }
    return visible;
}

// Players/<character>/Selector.png for all four characters, and Map.png and Airborne.png
// for Luigi, Toad and Toadette. Airborne columns follow AirbornePose in characters.h.
inline NamedImages reference_art(const Image &sheet) {
    if (sheet.width != ReferenceSheetWidth || sheet.height != ReferenceSheetHeight) return {};
    NamedImages result;
    static const char *const names[] = {"Mario", "Luigi", "Toad", "Toadette"};
    for (int character = 0; character < 4; ++character) {
        const int offset = character & 1 ? 981 : 0, top = character < 2 ? 272 : 2313;
        Image selector = blank_image(64, 32);
        if (!copy_cell(selector, 0, 0, sheet, offset + 524, top - 60, top - 28) ||
            !copy_cell(selector, 32, 0, sheet, offset + 268, top, top + 32)) return {};
        result.emplace_back(std::string("Players/") + names[character] + "/Selector.png", std::move(selector));
    }
    static constexpr int columns[] = {1, 429, 499, 668, 429, 668, 499};
    static constexpr int airborne_rows[] = {24, 212, 484, 751, 1114, 1423, 1786};
    // Zero keeps the standard pose for that AirbornePose.
    static constexpr int standard[] = {351, 384, 421};
    static constexpr int tail[] = {454, 487, 0, 697, 730, 763, 524, 557, 590};
    static constexpr int luigi_rows[] = {199, 471, 738, 1101, 1410, 1773, 2040};
    static constexpr int toad_rows[] = {2240, 2512, 2779, 3142, 3451, 3814, 4081};
    for (int character = 1; character < 4; ++character) {
        const int offset = character & 1 ? 981 : 0, frog = character == 3 ? 606 : 429;
        const int *separators = character == 1 ? luigi_rows : toad_rows;
        Image map = blank_image(192, 224), airborne = blank_image(352, 224);
        for (int suit = 0; suit < 7; ++suit) {
            const int column = suit == 4 ? frog : columns[suit];
            std::vector<int> frames = {column, column + 33};
            if (suit == 0) frames.insert(frames.end(), {71, 104, 137, 170});
            for (size_t frame = 0; frame < frames.size(); ++frame) {
                const int x = offset + frames[frame], bottom = separators[suit] - 1;
                int top = bottom;
                while (bottom - top < 32 && cell_background(sheet.at(x, top - 1))) --top;
                const int height = bottom - top;
                if ((height != 16 && height != 24 && height != 26) ||
                    !copy_cell(map, static_cast<int>(frame) * 32, suit * 32 + 32 - height, sheet, x, top, bottom))
                    return {};
            }
            const int row = airborne_rows[suit] + (character == 1 ? 0 : 2041), height = suit ? 32 : 16;
            std::vector<int> poses(standard, standard + 3);
            if (suit == 0) poses = {285, 0, 355};
            else if (suit == 3 || suit == 5) poses.assign(tail, tail + 9);
            else if (suit == 4) poses = {244};
            for (size_t frame = 0; frame < poses.size(); ++frame)
                if (poses[frame] && !copy_cell(airborne, static_cast<int>(frame) * 32, suit * 32 + 32 - height,
                                               sheet, offset + poses[frame], row, row + height))
                    return {};
            if (suit == 2 || suit == 6)
                for (int frame = 0; frame < 2; ++frame)
                    if (!copy_cell(airborne, (9 + frame) * 32, suit * 32, sheet, offset + 1 + frame * 33, row + 55, row + 87))
                        return {};
        }
        const std::string directory = std::string("Players/") + names[character];
        result.emplace_back(directory + "/Map.png", std::move(map));
        result.emplace_back(directory + "/Airborne.png", std::move(airborne));
    }
    return result;
}

// Controls/Hints.png from Vryell's kb_dark_all.png and controller_xbox.png. Keyboard icons
// fill the top row and Xbox icons the bottom row, in HintIcon order (frontend.h).
inline Image hint_atlas(const Image &keyboard, const Image &controller) {
    if (keyboard.width != 256 || keyboard.height != 416 || controller.width != 112 || controller.height != 432)
        return {};
    // Each entry holds one or two 16-pixel icons; a negative x marks a single icon.
    static constexpr int cells[2][7][4] = {
        {{208, 192, 208, 224}, {208, 208, 208, 240}, {144, 384, -1, 0}, {144, 0, -1, 0},
         {144, 384, -1, 0}, {208, 112, -1, 0}, {144, 0, -1, 0}},
        {{16, 160, 48, 160}, {64, 160, 32, 160}, {48, 64, -1, 0}, {48, 96, -1, 0},
         {48, 176, -1, 0}, {48, 0, -1, 0}, {48, 352, -1, 0}}};
    Image atlas = blank_image(224, 32);
    for (int row = 0; row < 2; ++row) {
        const Image &sheet = row ? controller : keyboard;
        for (int index = 0; index < 7; ++index) {
            const int *cell = cells[row][index];
            const int icons = cell[2] < 0 ? 1 : 2, left = 32 * index + (icons == 1 ? 8 : 0);
            for (int icon = 0; icon < icons; ++icon)
                for (int y = 0; y < 16; ++y)
                    for (int x = 0; x < 16; ++x)
                        atlas.pixels[(row * 16 + y) * atlas.width + left + icon * 16 + x] =
                            sheet.at(cell[icon * 2] + x, cell[icon * 2 + 1] + y);
        }
    }
    return atlas;
}
}
