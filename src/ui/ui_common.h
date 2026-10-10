#ifndef UI_COMMON_H
#define UI_COMMON_H

#include "renderer.h"
#include <string>

enum AppState {
    STATE_TITLE = 0,
    STATE_MAIN_MENU,
    STATE_SONG_SELECT,
    STATE_DIFFICULTY_SELECT,
    STATE_OPTIONS,
    STATE_REMAPPER,
    STATE_GAMEPLAY,
    STATE_PAUSE,
    STATE_RESULTS
};

void drawHeaderBar(const std::string& title, const std::string& subtitle = "");

#endif // UI_COMMON_H
