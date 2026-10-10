#include "config.h"
#include <cstdio>
#include <cstring>

GameConfig g_config;

GameConfig::GameConfig() {
    resetToDefaults();
}

void GameConfig::resetToDefaults() {
    // Default Fret mappings:
    // Left D-Pad  -> Green
    // Down D-Pad  -> Red (Up also allowed by default)
    // Triangle    -> Yellow
    // Circle      -> Blue
    // Cross       -> Orange
    buttons[ACTION_FRET_GREEN]  = PSP_CTRL_LEFT;
    buttons[ACTION_FRET_RED]    = PSP_CTRL_DOWN;
    buttons[ACTION_FRET_YELLOW] = PSP_CTRL_TRIANGLE;
    buttons[ACTION_FRET_BLUE]   = PSP_CTRL_CIRCLE;
    buttons[ACTION_FRET_ORANGE] = PSP_CTRL_CROSS;

    buttons[ACTION_STRUM_UP]    = PSP_CTRL_UP;
    buttons[ACTION_STRUM_DOWN]  = PSP_CTRL_DOWN;
    buttons[ACTION_STAR_POWER]  = PSP_CTRL_SELECT;
    buttons[ACTION_PAUSE]       = PSP_CTRL_START;

    note_speed = 3;
    strum_required = false; // Tap frets by default (portable PSP ergonomics)
    autoplay = false;
    audio_offset_ms = 0;
    background_fx = true;
}

double GameConfig::getTravelTime() const {
    // Look times matching prototype: [2.4, 2.0, 1.7, 1.45, 1.2]
    static const double LOOK[5] = { 2.4, 2.0, 1.7, 1.45, 1.2 };
    int idx = note_speed - 1;
    if (idx < 0) idx = 0;
    if (idx > 4) idx = 4;
    return LOOK[idx];
}

bool GameConfig::load(const std::string& filepath) {
    FILE* f = std::fopen(filepath.c_str(), "rb");
    if (!f) return false;

    uint32_t magic = 0;
    if (std::fread(&magic, sizeof(magic), 1, f) != 1 || magic != 0x50535048) { // "PSPH"
        std::fclose(f);
        return false;
    }

    if (std::fread(buttons, sizeof(uint32_t), ACTION_COUNT, f) != ACTION_COUNT) {
        std::fclose(f);
        return false;
    }

    if (std::fread(&note_speed, sizeof(int), 1, f) != 1) note_speed = 3;
    if (std::fread(&strum_required, sizeof(bool), 1, f) != 1) strum_required = false;
    if (std::fread(&autoplay, sizeof(bool), 1, f) != 1) autoplay = false;
    if (std::fread(&audio_offset_ms, sizeof(int), 1, f) != 1) audio_offset_ms = 0;
    if (std::fread(&background_fx, sizeof(bool), 1, f) != 1) background_fx = true;

    std::fclose(f);
    return true;
}

bool GameConfig::save(const std::string& filepath) const {
    FILE* f = std::fopen(filepath.c_str(), "wb");
    if (!f) return false;

    uint32_t magic = 0x50535048; // "PSPH"
    std::fwrite(&magic, sizeof(magic), 1, f);
    std::fwrite(buttons, sizeof(uint32_t), ACTION_COUNT, f);
    std::fwrite(&note_speed, sizeof(int), 1, f);
    std::fwrite(&strum_required, sizeof(bool), 1, f);
    std::fwrite(&autoplay, sizeof(bool), 1, f);
    std::fwrite(&audio_offset_ms, sizeof(int), 1, f);
    std::fwrite(&background_fx, sizeof(bool), 1, f);

    std::fclose(f);
    return true;
}

const char* GameConfig::getActionName(ActionType action) {
    switch (action) {
        case ACTION_FRET_GREEN:  return "Green Fret";
        case ACTION_FRET_RED:    return "Red Fret";
        case ACTION_FRET_YELLOW: return "Yellow Fret";
        case ACTION_FRET_BLUE:   return "Blue Fret";
        case ACTION_FRET_ORANGE: return "Orange Fret";
        case ACTION_STRUM_UP:    return "Strum Up";
        case ACTION_STRUM_DOWN:  return "Strum Down";
        case ACTION_STAR_POWER:  return "Star Power";
        case ACTION_PAUSE:       return "Pause";
        default:                 return "Unknown";
    }
}

std::string GameConfig::getButtonName(uint32_t button_mask) {
    if (button_mask == 0) return "None";

    std::string result;
    if (button_mask & PSP_CTRL_LEFT)     result += (result.empty() ? "" : "+") + std::string("LEFT");
    if (button_mask & PSP_CTRL_DOWN)     result += (result.empty() ? "" : "+") + std::string("DOWN");
    if (button_mask & PSP_CTRL_RIGHT)    result += (result.empty() ? "" : "+") + std::string("RIGHT");
    if (button_mask & PSP_CTRL_UP)       result += (result.empty() ? "" : "+") + std::string("UP");
    if (button_mask & PSP_CTRL_TRIANGLE) result += (result.empty() ? "" : "+") + std::string("TRIANGLE");
    if (button_mask & PSP_CTRL_CIRCLE)   result += (result.empty() ? "" : "+") + std::string("CIRCLE");
    if (button_mask & PSP_CTRL_CROSS)    result += (result.empty() ? "" : "+") + std::string("CROSS");
    if (button_mask & PSP_CTRL_SQUARE)   result += (result.empty() ? "" : "+") + std::string("SQUARE");
    if (button_mask & PSP_CTRL_LTRIGGER) result += (result.empty() ? "" : "+") + std::string("L TRIGGER");
    if (button_mask & PSP_CTRL_RTRIGGER) result += (result.empty() ? "" : "+") + std::string("R TRIGGER");
    if (button_mask & PSP_CTRL_START)    result += (result.empty() ? "" : "+") + std::string("START");
    if (button_mask & PSP_CTRL_SELECT)   result += (result.empty() ? "" : "+") + std::string("SELECT");

    return result.empty() ? "None" : result;
}
