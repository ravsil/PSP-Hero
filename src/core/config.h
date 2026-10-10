#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>
#include <string>

#ifdef __PSP__
#include <pspctrl.h>
#else
// Mock PSP buttons for host / testing
#define PSP_CTRL_SELECT      0x000001
#define PSP_CTRL_START       0x000008
#define PSP_CTRL_UP          0x000010
#define PSP_CTRL_RIGHT       0x000020
#define PSP_CTRL_DOWN        0x000040
#define PSP_CTRL_LEFT        0x000080
#define PSP_CTRL_LTRIGGER    0x000100
#define PSP_CTRL_RTRIGGER    0x000200
#define PSP_CTRL_TRIANGLE    0x001000
#define PSP_CTRL_CIRCLE      0x002000
#define PSP_CTRL_CROSS       0x004000
#define PSP_CTRL_SQUARE      0x008000
#endif

enum ActionType {
    ACTION_FRET_GREEN = 0,
    ACTION_FRET_RED,
    ACTION_FRET_YELLOW,
    ACTION_FRET_BLUE,
    ACTION_FRET_ORANGE,
    ACTION_STRUM_UP,
    ACTION_STRUM_DOWN,
    ACTION_STAR_POWER,
    ACTION_PAUSE,
    ACTION_COUNT
};

struct GameConfig {
    // Button mappings (masks of PSP_CTRL_*)
    uint32_t buttons[ACTION_COUNT];

    // Gameplay settings
    int note_speed;          // 1..5 (default: 3)
    bool strum_required;     // true: Strum required, false: All-Taps
    bool autoplay;           // Demonstration autoplay
    int audio_offset_ms;     // Calibration offset (-200..+200 ms)
    bool background_fx;      // Full effects vs Low effects

    GameConfig();

    void resetToDefaults();
    bool load(const std::string& filepath = "config.dat");
    bool save(const std::string& filepath = "config.dat") const;

    // Lookahead travel time in seconds corresponding to note_speed (1..5)
    double getTravelTime() const;

    // Helper to get action name string
    static const char* getActionName(ActionType action);

    // Helper to get button name string
    static std::string getButtonName(uint32_t button_mask);
};

// Global config instance
extern GameConfig g_config;

#endif // CONFIG_H
