#pragma once
#include <cstdint>

namespace smb3 {
enum class Screen { Notice, Intro, Main, Characters, Settings, Controls, Practice, Pause, Playing };
struct FrontendFlow {
    Screen screen = Screen::Notice, settings_return = Screen::Main;
    bool active = false, started = false;
    bool startup_title_seen = false, skip_intro = false;
    int players = 1, start_tick = -1, intro_tick = 0;
    bool shows_title() const {
        return screen == Screen::Main || (screen == Screen::Characters && settings_return == Screen::Main);
    }
    uint8_t intro_buttons(uint8_t mode, uint8_t title_state) const {
        // A second START in the native menu would launch the campaign.
        return skip_intro && mode == 0x20 && title_state < 2 && intro_tick % 4 == 0 ? 0x10 : 0;
    }
    void play() {
        if (!started) { start_tick = 0; startup_title_seen = false; }
        started = true;
        screen = Screen::Playing;
    }
    void loaded() { screen = Screen::Playing; started = true; start_tick = -1; }
    uint8_t startup_buttons(uint8_t mode, uint8_t title_state, uint8_t current_players) {
        const int tick = start_tick++;
        if (mode == 0x20) startup_title_seen = true;
        else {
            if (startup_title_seen) start_tick = -1;
            return 0;
        }
        // Intro skipping takes a different number of frames depending on when the player left it.
        if (title_state == 3) return tick % 4 ? 0 : current_players == players - 1 ? 0x10 : 0x20;
        return title_state < 3 && tick >= 12 && tick % 32 == 12 ? 0x10 : 0;
    }
};
}
