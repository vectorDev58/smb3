#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace smb3 {

struct PracticeLevel {
    uint8_t world, y, screen_column, entry;
    std::pair<const char *, const char *> name;
    int number, ordinal;

    std::string label(bool spanish) const {
        if (number) return std::to_string(world + 1) + "-" + std::to_string(number);
        std::string text = spanish ? name.second : name.first;
        if (ordinal) text += " " + std::to_string(ordinal);
        return text;
    }
};

using PracticeLevels = std::array<std::vector<PracticeLevel>, 8>;

inline PracticeLevels practice_levels(const std::vector<uint8_t> &rom) {
    PracticeLevels worlds;
    constexpr size_t bank_size = 8192, bank_offset = 12 * bank_size;
    if (rom.size() < bank_offset + bank_size) return worlds;
    const auto *bank = rom.data() + bank_offset;
    auto pointer = [bank](int table, int world) {
        const int index = table - 0xa000 + world * 2;
        return (bank[index] | (bank[index + 1] << 8)) - 0xa000;
    };
    for (int world = 0; world < static_cast<int>(worlds.size()); ++world) {
        const int map = pointer(0xa598, world);
        const int rows = pointer(0xb3dc, world);
        const int columns = pointer(0xb3ee, world);
        // The game's parallel row/type and screen/column lists have equal lengths.
        const int count = columns - rows;
        if (map < 0 || rows < 0 || count < 1 || count > 255 ||
            columns + count > static_cast<int>(bank_size)) continue;
        int map_cells = 0;
        while (map_cells < 4 * 144 && map + map_cells < static_cast<int>(bank_size) &&
               bank[map + map_cells] != 0xff) ++map_cells;
        int forts = 0, pipes = 0, houses = 0, plants = 0, traps = 0;
        auto &levels = worlds[world];
        for (int i = 0; i < count; ++i) {
            const uint8_t y = bank[rows + i] & 0xf0, position = bank[columns + i];
            const int tileset = bank[rows + i] & 0x0f;
            const int page = position >> 4, row = (y >> 4) - 2;
            const int cell = page * 144 + row * 16 + (position & 15);
            if (page > 3 || row < 0 || row >= 9 || cell >= map_cells || !tileset) continue;
            const uint8_t tile = bank[map + cell];
            PracticeLevel level{static_cast<uint8_t>(world), y, position, 0, {}, 0, 0};
            if (tile >= 0x03 && tile <= 0x0c) level.number = tile - 2;
            else if (tile == 0x67 || tile == 0xeb) {
                level.name = {"Fortress", "Fortaleza"};
                level.ordinal = ++forts;
            } else if (tile == 0xc9) level.name = {"King's castle", "Castillo del rey"};
            else if (tile == 0xcc) level.name = {"Bowser's castle", "Castillo de Bowser"};
            else if (tile == 0x5f) level.name = {"Spiral tower", "Torre espiral"};
            else if (tile == 0x68) level.name = {"Sand trap", "Trampa de arena"};
            else if (tile == 0x69) level.name = {"Pyramid", "Pirámide"};
            else if (tileset == 5 && world == 6) {
                level.name = {"Piranha plants", "Plantas"};
                level.ordinal = ++plants;
            } else if (tile == 0xe6) {
                level.name = {"Ambush", "Emboscada"};
                level.ordinal = ++traps;
            } else if (tileset == 10 && world == 7) {
                if (page == 1) level.name = {"Airship", "Dirigible"};
                else if (tile == 0xaf && page != 3) level.name = {"Ships", "Barcos"};
                else {
                    level.name = {"Tanks", "Tanques"};
                    level.ordinal = page == 3 ? 2 : 1;
                }
            } else if (tile == 0xbc) {
                level.name = {"Pipe", "Tubería"};
                level.ordinal = ++pipes;
            } else if (tileset == 7) {
                level.name = {"Toad house", "Casa de Toad"};
                level.ordinal = ++houses;
            } else continue;
            levels.push_back(level);
            // The map's native airship entry override also works from the king's castle.
            if (tile == 0xc9)
                levels.push_back({level.world, y, position, 2, {"Airship", "Dirigible"}, 0, 0});
        }
        std::stable_sort(levels.begin(), levels.end(), [](const PracticeLevel &a, const PracticeLevel &b) {
            return (a.number ? a.number : 99) < (b.number ? b.number : 99);
        });
    }
    return worlds;
}

}
