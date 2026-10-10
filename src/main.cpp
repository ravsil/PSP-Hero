#include "chart_parser.h"
#include "audio_player.h"
#include "highway_renderer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>

#ifdef __PSP__
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <psppower.h>

PSP_MODULE_INFO("CloneHeroPSP", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

static int g_exit_requested = 0;

static int exitCallback(int arg1, int arg2, void* common) {
    (void)arg1; (void)arg2; (void)common;
    g_exit_requested = 1;
    return 0;
}

static int callbackThread(SceSize args, void* argp) {
    (void)args; (void)argp;
    int cbid = sceKernelCreateCallback("Exit Callback", exitCallback, nullptr);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static int setupCallbacks() {
    int thid = sceKernelCreateThread("callback_thread", callbackThread, 0x11, 0xFA0, 0, nullptr);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, nullptr);
    }
    return thid;
}
#else
static int g_exit_requested = 0;
#endif

// Search for a file across common PSP working directories
static std::string findFilePath(const std::vector<std::string>& candidates) {
    for (const auto& path : candidates) {
        FILE* f = std::fopen(path.c_str(), "rb");
        if (f) {
            std::fclose(f);
            return path;
        }
    }
    return "";
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

#ifdef __PSP__
    setupCallbacks();
    pspDebugScreenInit();

    // Set PSP CPU clock to maximum 333MHz for smooth audio decoding and rendering
    scePowerSetClockFrequency(333, 333, 166);

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
#endif

    // Locate chart file
    std::vector<std::string> chart_candidates = {
        "song/Aerials/notes.chart",
        "song/notes.chart",
        "notes.chart",
        "ms0:/PSP/GAME/cloneHero/song/Aerials/notes.chart",
        "ms0:/PSP/GAME/cloneHero/song/notes.chart",
        "ms0:/PSP/GAME/cloneHero/notes.chart",
        "./song/Aerials/notes.chart",
        "./song/notes.chart"
    };

    std::string chart_path = findFilePath(chart_candidates);
    if (chart_path.empty()) {
#ifdef __PSP__
        pspDebugScreenPrintf("========================================\n");
        pspDebugScreenPrintf("  Clone Hero PSP - File Error           \n");
        pspDebugScreenPrintf("========================================\n\n");
        pspDebugScreenPrintf("Could not find 'notes.chart'!\n");
        pspDebugScreenPrintf("Please place 'song/notes.chart' in the\n");
        pspDebugScreenPrintf("game folder.\n\n");
        pspDebugScreenPrintf("Press HOME to exit...\n");
        while (!g_exit_requested) {
            sceDisplayWaitVblankStart();
        }
        sceKernelExitGame();
#endif
        return 1;
    }

    ChartSong song;
    if (!ChartParser::loadChartFile(chart_path, song)) {
#ifdef __PSP__
        pspDebugScreenPrintf("Error: Failed to parse %s\n", chart_path.c_str());
        while (!g_exit_requested) { sceDisplayWaitVblankStart(); }
        sceKernelExitGame();
#endif
        return 1;
    }

    // Load companion song.ini
    std::vector<std::string> ini_candidates = {
        "song/Aerials/song.ini",
        "song/song.ini",
        "song.ini",
        "ms0:/PSP/GAME/cloneHero/song/Aerials/song.ini",
        "ms0:/PSP/GAME/cloneHero/song/song.ini",
        "ms0:/PSP/GAME/cloneHero/song.ini"
    };
    std::string ini_path = findFilePath(ini_candidates);
    if (!ini_path.empty()) {
        ChartParser::loadSongIni(ini_path, song);
    }

    // Select default track
    const ChartTrack* base_track = song.getDefaultTrack();
    if (!base_track) {
#ifdef __PSP__
        pspDebugScreenPrintf("Error: No note tracks found in chart file.\n");
        while (!g_exit_requested) { sceDisplayWaitVblankStart(); }
        sceKernelExitGame();
#endif
        return 1;
    }

    // Make a mutable copy of the active track to track hit/miss states during play
    ChartTrack active_track = *base_track;

    // Locate audio file
    std::vector<std::string> audio_candidates = {
        "song/Aerials/guitar.opus",
        "song/guitar.opus",
        "guitar.opus",
        "ms0:/PSP/GAME/cloneHero/song/Aerials/guitar.opus",
        "ms0:/PSP/GAME/cloneHero/song/guitar.opus",
        "ms0:/PSP/GAME/cloneHero/guitar.opus"
    };
    std::string audio_path = findFilePath(audio_candidates);

    // Initialize audio engine
    AudioPlayer audio;
    if (!audio_path.empty()) {
        audio.init(audio_path);
    } else {
        audio.init(""); // Uses high-precision fallback RTC timer
    }

    // Initialize hardware renderer
    HighwayRenderer renderer;
    if (!renderer.init()) {
#ifdef __PSP__
        pspDebugScreenPrintf("Error: Failed to initialize PSP GU.\n");
        while (!g_exit_requested) { sceDisplayWaitVblankStart(); }
        sceKernelExitGame();
#endif
        return 1;
    }

    // Start playback
    audio.play();

    GameplayStats stats;
    stats.autoplay = true; // Default to autoplay for demonstration

#ifdef __PSP__
    SceCtrlData old_pad;
    std::memset(&old_pad, 0, sizeof(old_pad));
#endif

    const double HIT_WINDOW = 0.120; // 120ms hit window

    while (!g_exit_requested) {
        uint32_t buttons_down = 0;
        uint32_t held_frets_mask = 0;
        bool is_paused = !audio.isPlaying();

#ifdef __PSP__
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(&pad, 1);

        buttons_down = pad.Buttons & ~old_pad.Buttons;
        old_pad = pad;

        // --- BUTTON HANDLING ---

        // START: Toggle Pause / Resume
        if (buttons_down & PSP_CTRL_START) {
            audio.togglePause();
        }

        // SELECT: Toggle Autoplay mode
        if (buttons_down & PSP_CTRL_SELECT) {
            stats.autoplay = !stats.autoplay;
        }

        // L / R Triggers: Seek -5s / +5s
        if (buttons_down & PSP_CTRL_LTRIGGER) {
            double cur = audio.getTimeSeconds();
            double target = (cur > 5.0) ? cur - 5.0 : 0.0;
            audio.seek(target);
            // Reset future notes hit state after rewind
            for (auto& n : active_track.notes) {
                if (n.time_seconds >= target) {
                    n.hit = false;
                    n.missed = false;
                }
            }
        }
        if (buttons_down & PSP_CTRL_RTRIGGER) {
            double cur = audio.getTimeSeconds();
            audio.seek(cur + 5.0);
        }

        // SQUARE: Restart song from beginning when paused
        if (is_paused && (buttons_down & PSP_CTRL_SQUARE)) {
            audio.seek(0.0);
            for (auto& n : active_track.notes) {
                n.hit = false;
                n.missed = false;
            }
            stats.score = 0;
            stats.streak = 0;
            stats.multiplier = 1;
            stats.notes_hit = 0;
            stats.notes_missed = 0;
        }

        // Map buttons to frets:
        // Left D-Pad  -> Green (0)
        // Down / Up D-Pad -> Red (1)
        // Triangle    -> Yellow (2)
        // Circle      -> Blue (3)
        // Cross       -> Orange (4)
        if (pad.Buttons & PSP_CTRL_LEFT)                   held_frets_mask |= (1u << LANE_GREEN);
        if (pad.Buttons & (PSP_CTRL_DOWN | PSP_CTRL_UP))   held_frets_mask |= (1u << LANE_RED);
        if (pad.Buttons & PSP_CTRL_TRIANGLE)               held_frets_mask |= (1u << LANE_YELLOW);
        if (pad.Buttons & PSP_CTRL_CIRCLE)                 held_frets_mask |= (1u << LANE_BLUE);
        if (pad.Buttons & PSP_CTRL_CROSS)                  held_frets_mask |= (1u << LANE_ORANGE);
#endif

        double current_time = audio.getTimeSeconds();

        // --- GAMEPLAY HIT DETECTION ---

        if (stats.autoplay) {
            // Autoplay mode: Automatically strike notes when they cross the strikeline
            for (size_t i = 0; i < active_track.notes.size(); ++i) {
                auto& note = active_track.notes[i];
                if (!note.hit && current_time >= note.time_seconds) {
                    note.hit = true;
                    if (note.lane >= 0 && note.lane < 5) {
                        renderer.triggerHitSpark(note.lane);
                        held_frets_mask |= (1u << note.lane);
                    }
                    stats.notes_hit++;
                    stats.streak++;
                    if (stats.streak > stats.max_streak) stats.max_streak = stats.streak;
                    stats.multiplier = 1 + std::min<uint32_t>(stats.streak / 10, 3);
                    stats.score += 50 * stats.multiplier;
                }
            }
        } else {
            // Manual mode: Check user input against notes in the hit window
            if (buttons_down) {
                for (int lane = 0; lane < 5; ++lane) {
                    bool lane_pressed = false;
                    if (lane == LANE_GREEN  && (buttons_down & PSP_CTRL_LEFT))                 lane_pressed = true;
                    if (lane == LANE_RED    && (buttons_down & (PSP_CTRL_DOWN | PSP_CTRL_UP))) lane_pressed = true;
                    if (lane == LANE_YELLOW && (buttons_down & PSP_CTRL_TRIANGLE))             lane_pressed = true;
                    if (lane == LANE_BLUE   && (buttons_down & PSP_CTRL_CIRCLE))               lane_pressed = true;
                    if (lane == LANE_ORANGE && (buttons_down & PSP_CTRL_CROSS))                lane_pressed = true;

                    if (lane_pressed) {
                        // Find closest unhit note on this lane within hit window
                        for (auto& note : active_track.notes) {
                            if (!note.hit && !note.missed && note.lane == lane) {
                                double dt = std::abs(note.time_seconds - current_time);
                                if (dt <= HIT_WINDOW) {
                                    note.hit = true;
                                    renderer.triggerHitSpark(lane);
                                    stats.notes_hit++;
                                    stats.streak++;
                                    if (stats.streak > stats.max_streak) stats.max_streak = stats.streak;
                                    stats.multiplier = 1 + std::min<uint32_t>(stats.streak / 10, 3);
                                    stats.score += 50 * stats.multiplier;
                                    break;
                                }
                            }
                        }
                    }
                }

                // Open note check (Lane 7) with Square button
                if (buttons_down & PSP_CTRL_SQUARE) {
                    for (auto& note : active_track.notes) {
                        if (!note.hit && !note.missed && note.lane == LANE_OPEN) {
                            double dt = std::abs(note.time_seconds - current_time);
                            if (dt <= HIT_WINDOW) {
                                note.hit = true;
                                stats.notes_hit++;
                                stats.streak++;
                                if (stats.streak > stats.max_streak) stats.max_streak = stats.streak;
                                stats.multiplier = 1 + std::min<uint32_t>(stats.streak / 10, 3);
                                stats.score += 50 * stats.multiplier;
                                break;
                            }
                        }
                    }
                }
            }

            // Miss detection: Notes that passed beyond the hit window without being hit
            for (auto& note : active_track.notes) {
                if (!note.hit && !note.missed && current_time > (note.time_seconds + HIT_WINDOW)) {
                    note.missed = true;
                    stats.notes_missed++;
                    stats.streak = 0;
                    stats.multiplier = 1;
                }
            }
        }

        // --- RENDER FRAME ---
        renderer.beginFrame();
        renderer.render(song, active_track, current_time, stats, is_paused, held_frets_mask);
        renderer.endFrame();
    }

    // Cleanup
    audio.shutdown();
    renderer.shutdown();

#ifdef __PSP__
    sceKernelExitGame();
#endif
    return 0;
}
