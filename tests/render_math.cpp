#include "pixels.h"
#include "backdrops.h"
#include "characters.h"
#include "frontend_flow.h"
#include "menu_art.h"
#include "practice.h"
#include "resource_packs.h"
#include "pad_choice.h"
#include <cassert>
#include <cstring>

static void check_practice_levels() {
    assert(smb3::practice_levels({})[0].empty());
    std::vector<uint8_t> rom(13 * 8192);
    auto *bank = rom.data() + 12 * 8192;
    bank[0x599] = 0xa1; // Map at $A100.
    bank[0x13dd] = 0xa2; // Four row/type entries at $A200.
    bank[0x13ee] = 4;
    bank[0x13ef] = 0xa2; // Four screen/column entries at $A204.
    const uint8_t entries[] = {0x21, 0x21, 0x21, 0x22, 0x01, 0x02, 0x03, 0x11};
    std::copy(std::begin(entries), std::end(entries), bank + 0x200);
    bank[0x101] = 0x04; // Level 2 comes first in the spatial order.
    bank[0x102] = 0x03;
    bank[0x103] = 0x47; // Ordinary path.
    bank[0x100 + 144 + 1] = 0xc9; // Palace on the second page.
    bank[0x100 + 288] = 0xff;
    const auto worlds = smb3::practice_levels(rom);
    assert(worlds[0].size() == 4);
    assert(worlds[0][0].label(false) == "1-1" && worlds[0][0].screen_column == 2);
    assert(worlds[0][0].label(true) == "1-1");
    assert(worlds[0][1].label(false) == "1-2" && worlds[0][1].screen_column == 1);
    assert(worlds[0][2].screen_column == 0x11 && worlds[0][2].entry == 0);
    assert(worlds[0][3].screen_column == 0x11 && worlds[0][3].entry == 2);
    assert(worlds[0][2].label(false) == "King's castle" && worlds[0][2].label(true) == "Castillo del rey");
    assert(worlds[0][3].label(false) == "Airship" && worlds[0][3].label(true) == "Dirigible");
    assert(worlds[1].empty());
}

static smb3::Image filled(int width, int height, uint32_t pixel) {
    smb3::Image image = smb3::blank_image(width, height);
    image.pixels.assign(image.pixels.size(), pixel);
    return image;
}

static void check_resource_packs() {
    using namespace smb3;
    // Only the exact sheet revisions are converted; anything else keeps the ROM poses.
    assert(campaign_art(filled(1961, 4246, 0xff282828)).empty());
    assert(reference_art(filled(1961, 4290, 0xff282828)).empty());
    assert(campaign_art(filled(CampaignSheetWidth, CampaignSheetHeight, 0xff849dbd)).empty()); // all backdrop
    const auto campaign = campaign_art(filled(CampaignSheetWidth, CampaignSheetHeight, 0xff282828));
    assert(campaign.size() == 3 && campaign[1].first == "Players/Toad/Campaign.png");
    const Image &toad = campaign[1].second;
    assert(toad.width == CampaignFrames * 32 && toad.height == 7 * 32);
    assert(toad.at(0x0b * 32, 15) == 0 && toad.at(0x0b * 32, 16) == 0xff1f1f1f); // small frame sits at the bottom
    assert(toad.at(0x27 * 32, 32 * 3 + 7) == 0 && toad.at(0x27 * 32, 32 * 3 + 8) == 0xff1f1f1f); // shrunk to 24 rows
    assert(campaign_color(0xf04060, 3) == 0xff50c8 && campaign_color(0xf04060, 2) == 0xf04060);
    assert(campaign_poses(0)[0x43].row == 182 && campaign_poses(0)[0x43].padding == 4);
    assert(campaign_poses(2)[0x1b].x == 104 && campaign_poses(2)[93].x == 236 && campaign_poses(6)[0x18].row == 2007);
    // Per-character layouts: Luigi's carry poses, Toadette's kicks and frog shoe, and the frog skid cell.
    assert(campaign_poses(1, 1)[0x2b].x == 821 && campaign_poses(0, 1)[0x42].x == 867 && campaign_poses(1, 2)[0x2d].x == 825);
    assert(campaign_poses(3, 3)[0x2d].x == 895 && campaign_poses(0, 3)[0x2e].x == 687 && campaign_poses(4, 3)[0x18].x == 178);
    assert(campaign_poses(4, 2)[0x30].x == 244 && campaign_poses(4, 2)[0x18].x == 1);

    assert(reference_art(filled(ReferenceSheetWidth, ReferenceSheetHeight, 0xff2f3368)).empty());

    Image keyboard = blank_image(256, 416), controller = blank_image(112, 432);
    keyboard.pixels[(384 + 5) * 256 + 144 + 3] = 0xff123456;    // third hint, a single key
    controller.pixels[(160 + 2) * 112 + 48 + 1] = 0xff654321; // first hint, second button
    assert(hint_atlas(controller, keyboard).empty());
    const Image hints = hint_atlas(keyboard, controller);
    assert(hints.width == 224 && hints.height == 32);
    assert(hints.at(2 * 32 + 8 + 3, 5) == 0xff123456 && hints.at(16 + 1, 16 + 2) == 0xff654321);
}

static void check_widescreen_effects() {
    // A score drawn at NES X 15 belongs to the goomba 271 pixels right of the screen origin, not to the left edge.
    assert(smb3::wrapped_effect_x(15, {20}, -85, 341) == 15);
    assert(smb3::wrapped_effect_x(15, {268}, -85, 341) == 271);
    assert(smb3::wrapped_effect_x(240, {-20}, -85, 341) == -16);
    assert(smb3::wrapped_effect_x(15, {}, -85, 341) == 15);
    // Copies outside the visible width are never chosen, even when nearer to an anchor.
    assert(smb3::wrapped_effect_x(100, {400}, -85, 341) == 100);

    const uint32_t console[64] = {0xff123456};
    smb3::classic_palette = console;
    assert(smb3::color(0) == 0xff123456 && smb3::sprite_color(64) == 0xff123456);
    smb3::classic_palette = nullptr;
    assert(smb3::color(0) != 0xff123456);

    assert(smb3::rainbow(0xff1f1f1f, 0) == 0xff1f1f1f); // Outlines stay dark.
    const uint32_t red = smb3::rainbow(0xffffffff, 0), green = smb3::rainbow(0xffffffff, 120);
    assert((red >> 16 & 255) > 200 && (red & 0xffff) == 0);
    assert((green >> 8 & 255) > 200 && (green & 0xff00ff) == 0);
}

static void check_pad_choice() {
    CycBindings bind{};
    bind.source[0] = 1; // Keyboard + controller
    bind.source[1] = 2; // Controller
    CycInputFrame frame{};
    frame.pad_count = 1;
    frame.pad_guid[0] = frame.pad_guid[1] = "alike";
    int ids[CYC_INPUT_MAX_PADS] = {10, 11}, picked[CYC_INPUT_PLAYERS] = {-1, -1}, coop_pad = -1, pad[CYC_INPUT_PLAYERS];
    smb3_choose_pads(&bind, &frame, ids, picked, false, &coop_pad, pad);
    assert(pad[0] == 0 && pad[1] == -1); // As NESRecomp: the keyboard player 1 also plays on it.
    smb3_choose_pads(&bind, &frame, ids, picked, true, &coop_pad, pad);
    assert(pad[0] == -1 && pad[1] == 0 && coop_pad == 10); // Co-op: the lone pad is player 2's.
    frame.pad_count = 2;
    ids[0] = 12; ids[1] = 10; // A pad plugged in later comes first in SDL's order.
    smb3_choose_pads(&bind, &frame, ids, picked, true, &coop_pad, pad);
    assert(pad[0] == 0 && pad[1] == 1); // Player 2 keeps its pad.
    smb3_choose_pads(&bind, &frame, ids, picked, false, &coop_pad, pad);
    assert(pad[0] == 0 && pad[1] == 1 && coop_pad == -1);
    picked[1] = 12; // Chosen in Controls: player 2 takes the first pad, player 1 the other.
    smb3_choose_pads(&bind, &frame, ids, picked, false, &coop_pad, pad);
    assert(pad[0] == 1 && pad[1] == 0);
    picked[1] = -1; // After a restart only the model is known: two alike pads keep their order.
    std::strcpy(bind.device[1], "alike");
    smb3_choose_pads(&bind, &frame, ids, picked, false, &coop_pad, pad);
    assert(pad[1] == 0 && pad[0] == 1);
    bind.device[1][0] = '\0';
    bind.source[0] = 2; // Player 2 on the keyboard: it gets the pad player 1 leaves.
    bind.source[1] = 1;
    smb3_choose_pads(&bind, &frame, ids, picked, true, &coop_pad, pad);
    assert(pad[0] == 0 && pad[1] == 1);
    bind.source[1] = 0; // No input: no pad, even a chosen one.
    picked[1] = 10;
    smb3_choose_pads(&bind, &frame, ids, picked, false, &coop_pad, pad);
    assert(pad[0] == 0 && pad[1] == -1);
}

int main() {
    check_pad_choice();
    check_practice_levels();
    check_resource_packs();
    check_widescreen_effects();
    smb3::FrontendFlow flow;
    assert(flow.intro_buttons(0x20, 0) == 0);
    flow.skip_intro = true;
    assert(flow.intro_buttons(0, 0) == 0);
    assert(flow.intro_buttons(0x20, 0) == 0x10);
    assert(flow.intro_buttons(0x20, 1) == 0x10);
    assert(flow.intro_buttons(0x20, 2) == 0);
    assert(flow.intro_buttons(0x20, 3) == 0);
    flow.play();
    assert(flow.startup_buttons(0, 0, 0) == 0);
    for (int tick = 1; tick <= 12; ++tick)
        assert(flow.startup_buttons(0x20, 0, 0) == (tick == 12 ? 0x10 : 0));
    for (int tick = 13; tick < 60; ++tick) assert(flow.startup_buttons(0x20, 6, 0) == 0);
    assert(flow.startup_buttons(0x20, 3, 0) == 0x10);
    flow.startup_buttons(0, 5, 0);
    assert(flow.start_tick == -1);
    flow.loaded();
    flow.play();
    assert(flow.start_tick == -1);
    smb3::FrontendFlow two_players;
    two_players.players = 2;
    two_players.play();
    assert(two_players.startup_buttons(0x20, 3, 0) == 0x20);
    for (int tick = 1; tick < 4; ++tick) assert(two_players.startup_buttons(0x20, 3, 1) == 0);
    assert(two_players.startup_buttons(0x20, 3, 1) == 0x10);
    assert(smb3::menu_art({}).title.empty());
    std::vector<uint8_t> menu_rom(393216);
    menu_rom[262144 + 94 * 1024 + 0x30 * 16] = 0x80;
    menu_rom[262144 + 94 * 1024 + 0x30 * 16 + 8] = 0x80;
    assert(smb3::menu_glyph(menu_rom, 'A')[16] == 0);
    assert(smb3::menu_glyph(menu_rom, 'A')[17] == 1);
    assert(smb3::menu_glyph(menu_rom, 'A') == smb3::menu_glyph(menu_rom, 'a'));
    assert(smb3::menu_glyph(menu_rom, 0xc1)[4] == 1);
    assert(smb3::menu_glyph(menu_rom, ' ')[16] == 0);
    for (unsigned digit = '0'; digit <= '9'; ++digit) {
        const auto glyph = smb3::menu_glyph(menu_rom, digit);
        assert(glyph == smb3::menu_glyph({}, digit));
        const auto pixels = std::count(glyph.begin(), glyph.end(), 1);
        assert(pixels >= 12 && pixels <= 32);
    }
    for (int entry = 8; entry < 34; ++entry) {
        menu_rom[25 * 8192 + 0x5da + entry * 2] = 0;
        menu_rom[25 * 8192 + 0x5da + entry * 2 + 1] = 0xc8;
    }
    // The title update stream places a tile in the logo's top-left letter region.
    const uint8_t update[] = {0x20, 0x8b, 1, 1, 0};
    std::copy(std::begin(update), std::end(update), menu_rom.begin() + 25 * 8192 + 0x800);
    menu_rom[262144 + 120 * 1024 + 16] = 0x80;
    const auto title = smb3::menu_art(menu_rom).title;
    assert(title.width == 176 && title.height == 120);
    assert(title.at(48, 0) == smb3::color(0x0f));
    assert(title.at(49, 0) == 0);
    menu_rom[25 * 8192 + 0x5da + 8 * 2 + 1] = 0xff;
    assert(smb3::menu_art(menu_rom).title.empty());
    std::array<uint32_t, 256> stage_row;
    for (size_t i = 0; i < stage_row.size(); ++i) stage_row[i] = i % smb3::StagePatternPeriod;
    stage_row[0] = 0xff;
    assert(smb3::stage_repeating_row(stage_row.data()));
    stage_row[128] = 0xff;
    assert(!smb3::stage_repeating_row(stage_row.data()));
    assert(smb3::stage_fabric(0xffe68644, -1) == smb3::stage_fabric(0xffe68644, 63));
    const uint32_t paper = smb3::color(0x37);
    assert(smb3::stage_backdrop(paper, 213, 108, 426, 192) == paper);
    assert(smb3::stage_backdrop(paper, 0, 108, 426, 192) != paper);
    assert(smb3::title_luigi_frame(0x00, true) == 0x06);
    assert(smb3::title_luigi_frame(0x00, false) == 0x0c);
    assert(smb3::title_luigi_frame(0x0d, false) == 0x41);
    assert(smb3::title_luigi_frame(0x0e, false) != smb3::title_luigi_frame(0x0f, false));
    assert(smb3::title_luigi_frame(0x14, false) == 0x4f);
    const uint8_t tile[16] = {0x80,0,0,0,0,0,0,1, 0x40,0,0,0,0,0,0,1};
    assert(smb3::tile_pixel(tile, 0, 0) == 1);
    assert(smb3::tile_pixel(tile, 1, 0) == 2);
    assert(smb3::tile_pixel(tile, 7, 7) == 3);
    assert(smb3::tile_pixel(tile, 2, 0) == 0);
    assert(smb3::blend(0xff123456, 0x00112233) == 0xff123456);
    assert(smb3::blend(0xff123456, 0xffaabbcc) == 0xffaabbcc);
    assert(smb3::wrap(-1, 512) == 511);
    assert(smb3::unwrap(114, 884) == 882);
    assert(smb3::unwrap(0, 255) == 256);
    assert(smb3::unwrap(255, 256) == 255);
    assert(smb3::unwrap(254, 0) == -2);
    assert(smb3::native_origin(0, 426, 2048) == 0);
    assert(smb3::native_origin(882, 426, 2048) == 85);
    assert(smb3::native_origin(1792, 426, 2048) == 170);
    assert(smb3::native_origin(0, 426, 256) == 85);
    assert(smb3::native_origin(0, 854, 512) == 171);
    for (int width : {256, 426, 560, 854}) {
        const int center = (width - 256) / 2;
        assert(smb3::level_origin(2560, width, 2816, 2688) == center);
        assert(smb3::level_origin(2520, width, 2816, 2688) == center);
        assert(smb3::level_origin(0, width, 256, 128) == center);
        assert(smb3::level_origin(192, width, 512, 384) == center);
    }
    assert(smb3::backdrop_height(239) == 0);
    assert(smb3::backdrop_height(159) == -80);
    std::array<uint8_t, 8192> room{};
    room[0x1b41] = 0x41;
    room[0x1b42] = 0x10;
    assert(smb3::goal_position(room.data(), room.size()) == 256);
    room[0x1b41] = 0xff;
    assert(smb3::goal_position(room.data(), room.size()) == -1);
    for (int width : {256, 426, 560, 854}) {
        const int origin = smb3::native_origin(882, width, 3840);
        assert(origin + 100 + smb3::parallax_offset(882 - origin, width, 10) == 73);
        for (int camera = 0; camera <= (width - 256) / 2; ++camera) {
            const int start_origin = smb3::native_origin(camera, width, 3840);
            assert(smb3::parallax_offset(camera - start_origin, width, 10) ==
                   smb3::parallax_offset(0, width, 10));
        }
    }
    assert(smb3::sprite_priority(0xff00ff00, 0xff123456, true, true) == 0);
    assert(smb3::sprite_priority(0xff00ff00, 0xff123456, true, false) == 0xff123456);
    assert(smb3::sprite_priority(0xff00ff00, 0, true, true) == 0xff00ff00);
    for (int next = 0; next < 3840; ++next)
        for (int delta : {-4, -1, 0, 1, 4}) {
            const int current = std::max(0, next + delta);
            assert(smb3::unwrap(current & 255, next) == current);
        }
    for (int width : {256, 426, 560, 854})
        for (int length : {256, 512, 2048, 3840})
            for (int camera = 0; camera <= length - 256; ++camera) {
                const int origin = smb3::native_origin(camera, width, length);
                assert(origin >= 0 && origin + 256 <= width);
                if (width <= length) assert(camera - origin >= 0 && camera - origin + width <= length);
            }
    assert(smb3::color(0x0f) == smb3::color(0x3f));
    std::vector<uint32_t> canvas(25, 0xffffffff);
    std::vector<uint8_t> mask(25, 0);
    std::vector<uint8_t> floor(25, 0);
    mask[0] = mask[2] = mask[3] = mask[6] = 1;
    floor[12] = 1;
    canvas[9] = 0xff000000;
    smb3::shadow(canvas.data(), 5, 5, mask, 1, 1, floor.data());
    assert(canvas[6] == 0xffffffff);
    assert(canvas[8] == 0xff999999);
    assert(canvas[9] == 0xff000000);
    assert(canvas[12] == 0xffffffff);
    smb3::Image sheet{256, 192, std::vector<uint32_t>(256 * 192)};
    sheet.pixels[32] = 0xff007eff;
    sheet.pixels[33] = 0xffffd9b2;
    sheet.pixels[34] = 0xff1f1f1f;
    const auto sprite = smb3::reference_sprite(sheet, 0, 1);
    assert(sprite[0] == 1 && sprite[1] == 2 && sprite[2] == 3 && sprite[3] == 0);
    assert(smb3::sprite_visible(sprite));
    assert(!smb3::sprite_visible(smb3::reference_sprite(sheet, 1, 1)));
    assert(!smb3::sprite_visible(smb3::reference_sprite({}, 6, 1)));
    assert(smb3::character_role(0xff2c180c, 3, true) == 3);
    assert(smb3::character_role(0xffffccab, 3, true) == 2);
    assert(smb3::character_role(0xffb86b22, 5, true) == 1);
    assert(smb3::reference_form(2, 0x0b) == 0);
    for (unsigned form = 0; form <= 6; ++form) {
        assert(smb3::campaign_frame(form, 0x05, true) == (form == 3 || form == 5 ? 0x03u : 0x05u));
        assert(smb3::campaign_frame(form, 0x05, false) == 0x05);
        assert(smb3::campaign_frame(form, 0x32, true) == 0x32);
    }
    const uint8_t map_palette[] = {0x0f, 0x16, 0x36, 0x0f};
    const uint8_t map_fade[] = {0x0f, 0x0f, 0x06, 0x0f};
    assert(smb3::sprite_palette_light(map_palette, map_palette) == 255);
    assert(smb3::sprite_palette_light(map_fade, map_palette) == 63);
    assert(smb3::reference_cell(0, 0x0b, false, false) == 0);
    assert(smb3::reference_cell(0, 0x4b, false, false) == 15);
    assert(smb3::reference_cell(1, 0x0f, false, false) == 1);
    assert(smb3::reference_cell(1, 0x0e, false, false) == 0);
    assert(smb3::reference_cell(0, 0x3e, true, false) != smb3::reference_cell(0, 0x3f, true, false));
    assert(smb3::reference_cell(1, 0x34, false, true) != smb3::reference_cell(1, 0x34, false, false));
    const uint8_t poof[] = {0x99, 0x45, 0xc1, 0x20, 0x99, 0x45, 0x01, 0x18};
    const uint8_t player[] = {0xa1, 0x1b, 0x40, 0x18, 0xa1, 0x19, 0x40, 0x20};
    assert(smb3::player_poof(poof) && !smb3::player_poof(player));
    assert(smb3::map_pose(0, 0x2d, 0, false, false) == 0);
    assert(smb3::map_pose(3, 0xb7, 0x40, false, false) == 1);
    assert(smb3::map_pose(4, 0xd3, 0, false, false) == 1);
    assert(smb3::map_pose(5, 0xc3, 0, false, false) == 1);
    assert(smb3::map_pose(6, 0xcf, 0x40, false, false) == 1);
    assert(smb3::map_pose(7, 0x9f, 0, false, true) == 1);
    assert(smb3::map_pose(8, 0xa7, 0, false, false) == 1);
    assert(smb3::map_pose(0, 0x3d, 0, true, false) == 4);
    assert(smb3::map_pose(0, 0x63, 3, false, false) == -1);
    using smb3::AirbornePose;
    for (unsigned suit : {3u, 5u}) {
        assert(smb3::airborne_pose(suit, 0x36, true, false, true, false) == AirbornePose::Fly1);
        assert(smb3::airborne_pose(suit, 0x37, true, false, true, false) == AirbornePose::Fly2);
        assert(smb3::airborne_pose(suit, 0x38, true, false, true, false) == AirbornePose::Fly3);
        assert(smb3::airborne_pose(suit, 0x08, true, false, false, false) == AirbornePose::Flutter1);
        assert(smb3::airborne_pose(suit, 0x09, true, false, false, false) == AirbornePose::Flutter2);
        assert(smb3::airborne_pose(suit, 0x0a, true, false, false, false) == AirbornePose::Flutter3);
        assert(smb3::airborne_pose(suit, 0x09, true, false, false, true) == AirbornePose::None);
        assert(smb3::airborne_pose(suit, 0x36, true, true, false, false) == AirbornePose::None);
        assert(smb3::airborne_pose(suit, 0x37, false, false, false, false) == AirbornePose::None);
        assert(smb3::airborne_pose(suit, 0x50, true, false, true, false) == AirbornePose::Jump);
        assert(smb3::airborne_pose(suit, 0x50, true, false, false, false) == AirbornePose::Fall);
    }
    for (unsigned suit : {1u, 2u, 6u}) {
        assert(smb3::airborne_pose(suit, 0x38, true, false, true, false) == AirbornePose::RunJump);
        assert(smb3::airborne_pose(suit, 0x4f, true, false, true, false) == AirbornePose::Jump);
        assert(smb3::airborne_pose(suit, 0x06, true, false, false, false) == AirbornePose::Fall);
        assert(smb3::airborne_pose(suit, 0x36, true, false, false, false) == AirbornePose::None);
    }
    assert(smb3::airborne_pose(0, 0x4e, true, false, true, false) == AirbornePose::RunJump);
    assert(smb3::airborne_pose(4, 0x17, true, false, true, false) == AirbornePose::Jump);
    assert(smb3::airborne_pose(4, 0x16, false, false, false, false) == AirbornePose::None);
    for (unsigned suit : {2u, 6u}) {
        assert(smb3::airborne_pose(suit, 0x36, true, false, true, true) == AirbornePose::ThrowReady);
        assert(smb3::airborne_pose(suit, 0x35, true, true, true, true) == AirbornePose::ThrowRelease);
        assert(smb3::airborne_pose(suit, 0x34, true, true, true, false) == AirbornePose::None);
    }
    smb3::Sprite body{};
    for (int y = 10; y <= 27; ++y) std::fill_n(body.begin() + 32 * y + 10, 12, 1);
    const auto luigi = smb3::luigi_silhouette(body);
    assert(!body[9 * 32 + 11] && luigi[9 * 32 + 11]);
    assert(body[27 * 32 + 10] && !luigi[27 * 32 + 10] && !luigi[28 * 32 + 11]);
    assert(smb3::sprite_color(0x2d) == smb3::color(0x2d));
    assert(smb3::dim(0x8081a0ff, 128) == 0x80415080);
    const auto cave = smb3::level_backdrop(0, 3, 3, 0x20, 0x0f, 0);
    assert(cave.scenery == smb3::Scenery::Underground && !cave.sky && !cave.fallback_clouds);
    const auto water = smb3::level_backdrop(2, 6, 6, 0x30, 0x12, 1);
    assert(water.atmosphere == smb3::Atmosphere::Bubbles && water.sky_frame_ticks == 30);
    assert(smb3::level_backdrop(5, 12, 12, 0x10, 0x31, 0).atmosphere == smb3::Atmosphere::Snow);
    const auto fort = smb3::level_backdrop(7, 2, 2, 0x40, 0x0f, 4);
    assert(fort.scenery == smb3::Scenery::Castle && !fort.sky && !fort.fallback_clouds);
    const auto tank = smb3::level_backdrop(7, 10, 21, 0x60, 0x0f, 6);
    assert(tank.atmosphere == smb3::Atmosphere::Embers && tank.sky_frame_ticks == 12 && !tank.fallback_clouds);
    const auto cabin = smb3::level_backdrop(7, 10, 10, 0x60, 0x0f, 5);
    assert(!cabin.sky && !cabin.hills && !cabin.fallback_clouds);
    const auto skyland = smb3::level_backdrop(4, 1, 13, 0x90, 0x3c, 0);
    assert(skyland.fallback_clouds && !skyland.hills);
    const auto hills = smb3::level_backdrop(0, 3, 19, 0x90, 0x3c, 0);
    assert(hills.scenery == smb3::Scenery::Grass && hills.fallback_clouds);
    const auto foreground = smb3::landscape_layer(hills, tile, false);
    const auto distant = smb3::landscape_layer(hills, tile, true);
    assert(foreground.at(13, 0) == 0 && foreground.at(13, 90) >> 24 == 255);
    assert(foreground.pixels != distant.pixels);
    assert(smb3::landscape_layer(cabin, tile, true).empty());
    const smb3::Image two_frames{1024, 512, {}};
    assert(smb3::backdrop_frame(two_frames, 11, 12).left == 0);
    assert(smb3::backdrop_frame(two_frames, 12, 12).left == 512);
    assert(smb3::backdrop_frame(two_frames, 12, 12).width == 512);
    assert(smb3::backdrop_frame(two_frames, 24, 12).left == 0);
    assert(smb3::backdrop_frame(sheet, 0, 12).width == 0);
    constexpr uint8_t sea_water[] = {0x02, 0x3f, 0x8a, 0xc0};
    constexpr uint8_t sea_solid[] = {0x0a, 0x4c, 0x91, 0xe2};
    constexpr uint8_t plains_water[] = {0xff, 0xff, 0xff, 0xda};
    constexpr uint8_t fortress_solid[] = {0x11, 0x5a, 0x9b, 0xe2};
    using smb3::WaterTile;
    assert(smb3::water_tile(6, 0x8a, sea_water, sea_solid) == WaterTile::Dry);
    assert(smb3::water_tile(6, 0x8b, sea_water, sea_solid) == WaterTile::SeaSurface);
    assert(smb3::water_tile(6, 0x8c, sea_water, sea_solid) == WaterTile::Fill);
    assert(smb3::water_tile(6, 0x8f, sea_water, sea_solid) == WaterTile::Submerged);
    assert(smb3::water_tile(2, 0x94, plains_water, fortress_solid) == WaterTile::Dry);
    assert(smb3::water_tile(2, 0xdc, plains_water, fortress_solid) == WaterTile::Wave);
    assert(smb3::water_partition(0xa0, 0xa0, false) && smb3::water_partition(0xa0, 0x40, false));
    assert(!smb3::water_partition(0xa0, 0x40, true) && !smb3::water_partition(0xc0, 0x00, false));
    constexpr uint32_t water_body = 0x884386ca, water_crest = 0xbcdcf6ff;
    assert(smb3::water_overlay(WaterTile::SeaSurface, 3, 2, water_body, water_crest) == 0);
    assert(smb3::water_overlay(WaterTile::SeaSurface, 0, 5, water_body, water_crest) == water_body);
    assert(smb3::water_overlay(WaterTile::Wave, 0, 3, water_body, water_crest) == 0);
    assert(smb3::water_overlay(WaterTile::Wave, 2, 2, water_body, water_crest) == water_crest);
    assert(smb3::water_overlay(WaterTile::Wave, 3, 3, water_body, water_crest) == water_body);
    assert(smb3::tile_animation(0x60) == smb3::TileAnimation::Question);
    assert(smb3::tile_animation(0x66) == smb3::TileAnimation::None);
    assert(smb3::tile_animation(0x70) == smb3::TileAnimation::Brick);
    assert(smb3::tile_animation(0x40) == smb3::TileAnimation::Coin);
    const auto question = smb3::TileAnimation::Question;
    assert(smb3::animated_tile_color(question, 0x0f, 1, 5, 2, 0) == smb3::color(0x0f));
    assert(smb3::animated_tile_color(question, 0x27, 3, 5, 2, 0) !=
           smb3::animated_tile_color(question, 0x27, 3, 5, 2, 1));
}
