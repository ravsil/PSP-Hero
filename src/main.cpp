#include "chart_parser.h"
#include "audio_player.h"
#include "song_manager.h"
#include "config.h"
#include "renderer.h"
#include "highway_renderer.h"
#include "gameplay.h"
#include "menu_screens.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

#ifdef __PSP__
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <psppower.h>
#include <psprtc.h>

PSP_MODULE_INFO("PSPHero", 0, 1, 0);
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

// High-precision clock in seconds
static double getRealTimeSeconds() {
#ifdef __PSP__
    u64 tick;
    sceRtcGetCurrentTick(&tick);
    u32 resolution = sceRtcGetTickResolution();
    return static_cast<double>(tick) / static_cast<double>(resolution);
#else
    return 0.0;
#endif
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

#ifdef __PSP__
    setupCallbacks();
    pspDebugScreenInit();

    // Set PSP CPU clock to maximum 333MHz
    scePowerSetClockFrequency(333, 333, 166);

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
#endif

    // Initialize configuration
    g_config.load();

    // Initialize hardware renderer
    if (!g_renderer.init()) {
#ifdef __PSP__
        pspDebugScreenPrintf("Error: Failed to initialize PSP GU.\n");
        while (!g_exit_requested) { sceDisplayWaitVblankStart(); }
        sceKernelExitGame();
#endif
        return 1;
    }

    // Scan for available songs in "songs/" (with automatic fallbacks)
    g_song_manager.scanSongs("songs");

    AudioPlayer audio;
    HighwayRenderer highway;
    GameSession session;

    ChartSong current_chart_song;
    ChartTrack current_active_track;
    std::string current_diff_name = "Expert";
    size_t current_song_idx = 0;
    bool is_new_best = false;

    // Menu state variables
    AppState state = STATE_TITLE;
    int menu_idx = 0;
    int song_scroll_offset = 0;
    int selected_diff_idx = 0;
    int options_idx = 0;
    int remapper_idx = 0;
    bool is_listening_button = false;
    int pause_idx = 0;

    double last_time = getRealTimeSeconds();

#ifdef __PSP__
    SceCtrlData old_pad;
    std::memset(&old_pad, 0, sizeof(old_pad));
#endif

    const double HIT_WINDOW = 0.120; // 120ms hit window

    while (!g_exit_requested) {
        double now = getRealTimeSeconds();
        float dt = static_cast<float>(now - last_time);
        if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;
        last_time = now;

        uint32_t buttons_down = 0;
        uint32_t buttons_held = 0;

#ifdef __PSP__
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(&pad, 1);
        buttons_held = pad.Buttons;
        buttons_down = pad.Buttons & ~old_pad.Buttons;
        old_pad = pad;
#endif

        // Global background animation tick
        g_renderer.updateBackground(dt, 120.0f, false);

        // --- STATE MACHINE DISPATCH ---

        if (state == STATE_TITLE) {
            if (buttons_down & (PSP_CTRL_START | PSP_CTRL_CROSS)) {
                state = STATE_MAIN_MENU;
                menu_idx = 0;
            }

            g_renderer.beginFrame();
            MenuScreens::renderTitle(static_cast<float>(now));
            g_renderer.endFrame();
        }
        else if (state == STATE_MAIN_MENU) {
            // Navigation
            if (buttons_down & PSP_CTRL_UP) {
                menu_idx = (menu_idx - 1 + 3) % 3;
            }
            if (buttons_down & PSP_CTRL_DOWN) {
                menu_idx = (menu_idx + 1) % 3;
            }

            // Confirm selection
            if (buttons_down & PSP_CTRL_CROSS) {
                if (menu_idx == 0) {
                    // Quick Play
                    if (g_song_manager.getSongCount() > 0) {
                        current_song_idx = 0;
                        selected_diff_idx = 0;
                        state = STATE_DIFFICULTY_SELECT;
                    }
                } else if (menu_idx == 1) {
                    // Song Select
                    state = STATE_SONG_SELECT;
                } else if (menu_idx == 2) {
                    // Options
                    state = STATE_OPTIONS;
                    options_idx = 0;
                }
            }

            // (NOTE: NO QUIT BUTTON ON MAIN MENU!)

            g_renderer.beginFrame();
            MenuScreens::renderMainMenu(menu_idx);
            g_renderer.endFrame();
        }
        else if (state == STATE_SONG_SELECT) {
            size_t count = g_song_manager.getSongCount();

            if (buttons_down & PSP_CTRL_UP) {
                if (current_song_idx > 0) {
                    current_song_idx--;
                    if (static_cast<int>(current_song_idx) < song_scroll_offset) {
                        song_scroll_offset = static_cast<int>(current_song_idx);
                    }
                }
            }
            if (buttons_down & PSP_CTRL_DOWN) {
                if (current_song_idx + 1 < count) {
                    current_song_idx++;
                    if (static_cast<int>(current_song_idx) >= song_scroll_offset + 4) {
                        song_scroll_offset = static_cast<int>(current_song_idx) - 3;
                    }
                }
            }

            if (buttons_down & PSP_CTRL_CROSS && count > 0) {
                selected_diff_idx = 0;
                state = STATE_DIFFICULTY_SELECT;
            }

            if (buttons_down & PSP_CTRL_CIRCLE) {
                state = STATE_MAIN_MENU;
            }

            g_renderer.beginFrame();
            MenuScreens::renderSongSelect(g_song_manager.getSongs(), current_song_idx, song_scroll_offset);
            g_renderer.endFrame();
        }
        else if (state == STATE_DIFFICULTY_SELECT) {
            const SongItem* song = g_song_manager.getSong(current_song_idx);
            std::vector<std::string> diffs;
            if (song) {
                diffs = song->getAvailableDifficulties();
            }
            int diff_count = static_cast<int>(diffs.size());
            if (diff_count > 0 && selected_diff_idx >= diff_count) {
                selected_diff_idx = diff_count - 1;
            }

            if (diff_count > 0) {
                if (buttons_down & PSP_CTRL_UP) {
                    selected_diff_idx = (selected_diff_idx - 1 + diff_count) % diff_count;
                }
                if (buttons_down & PSP_CTRL_DOWN) {
                    selected_diff_idx = (selected_diff_idx + 1) % diff_count;
                }
            }

            if (buttons_down & PSP_CTRL_CIRCLE) {
                state = STATE_SONG_SELECT;
            }

            if (buttons_down & PSP_CTRL_CROSS && song && diff_count > 0) {
                std::string chosen_diff = diffs[selected_diff_idx];

                // Load chart file
                if (ChartParser::loadChartFile(song->chart_path, current_chart_song)) {
                    if (!song->ini_path.empty()) {
                        ChartParser::loadSongIni(song->ini_path, current_chart_song);
                    }

                    // Find track
                    const ChartTrack* track = nullptr;
                    for (const auto& t : current_chart_song.tracks) {
                        if (t.difficulty == chosen_diff) {
                            if (t.instrument == "Single" || t.instrument == "Guitar") {
                                track = &t;
                                break;
                            }
                            if (!track) track = &t;
                        }
                    }
                    if (!track) track = current_chart_song.getDefaultTrack();

                    if (track) {
                        current_active_track = *track;
                        current_diff_name = track->difficulty;

                        // Initialize audio
                        audio.init(song->audio_path);
                        audio.play();

                        session.reset();
                        is_new_best = false;
                        state = STATE_GAMEPLAY;
                    }
                }
            }

            g_renderer.beginFrame();
            if (song) {
                MenuScreens::renderDifficultySelect(*song, selected_diff_idx);
            }
            g_renderer.endFrame();
        }
        else if (state == STATE_OPTIONS) {
            if (buttons_down & PSP_CTRL_UP) {
                options_idx = (options_idx - 1 + 6) % 6;
            }
            if (buttons_down & PSP_CTRL_DOWN) {
                options_idx = (options_idx + 1) % 6;
            }

            // Adjust values Left/Right
            int dir = 0;
            if (buttons_down & PSP_CTRL_LEFT) dir = -1;
            if (buttons_down & PSP_CTRL_RIGHT) dir = 1;

            if (dir != 0) {
                switch (options_idx) {
                    case 1:
                        g_config.note_speed = std::clamp(g_config.note_speed + dir, 1, 5);
                        break;
                    case 2:
                        g_config.strum_required = !g_config.strum_required;
                        break;
                    case 3:
                        g_config.autoplay = !g_config.autoplay;
                        break;
                    case 4:
                        g_config.audio_offset_ms = std::clamp(g_config.audio_offset_ms + dir * 10, -200, 200);
                        break;
                    case 5:
                        g_config.background_fx = !g_config.background_fx;
                        break;
                }
                g_config.save();
            }

            if (buttons_down & PSP_CTRL_CROSS) {
                if (options_idx == 0) {
                    state = STATE_REMAPPER;
                    remapper_idx = 0;
                    is_listening_button = false;
                } else if (options_idx == 1) {
                    g_config.note_speed = (g_config.note_speed % 5) + 1;
                    g_config.save();
                } else if (options_idx == 2) {
                    g_config.strum_required = !g_config.strum_required;
                    g_config.save();
                } else if (options_idx == 3) {
                    g_config.autoplay = !g_config.autoplay;
                    g_config.save();
                } else if (options_idx == 5) {
                    g_config.background_fx = !g_config.background_fx;
                    g_config.save();
                }
            }

            if (buttons_down & PSP_CTRL_CIRCLE) {
                g_config.save();
                state = STATE_MAIN_MENU;
            }

            g_renderer.beginFrame();
            MenuScreens::renderOptions(options_idx);
            g_renderer.endFrame();
        }
        else if (state == STATE_REMAPPER) {
            const int total_rows = ACTION_COUNT + 1;

            if (is_listening_button) {
                if (buttons_down != 0) {
                    // Assign the pressed button mask (works for any button including Circle)
                    g_config.buttons[remapper_idx] = buttons_down;
                    g_config.save();
                    is_listening_button = false;
                }
            } else {
                if (buttons_down & PSP_CTRL_UP) {
                    remapper_idx = (remapper_idx - 1 + total_rows) % total_rows;
                }
                if (buttons_down & PSP_CTRL_DOWN) {
                    remapper_idx = (remapper_idx + 1) % total_rows;
                }

                if (buttons_down & PSP_CTRL_CROSS) {
                    if (remapper_idx == ACTION_COUNT) {
                        g_config.resetToDefaults();
                        g_config.save();
                    } else {
                        is_listening_button = true;
                    }
                }

                if (buttons_down & PSP_CTRL_CIRCLE) {
                    g_config.save();
                    state = STATE_OPTIONS;
                }
            }

            g_renderer.beginFrame();
            MenuScreens::renderRemapper(remapper_idx, is_listening_button);
            g_renderer.endFrame();
        }
        else if (state == STATE_GAMEPLAY) {
            double current_time = audio.getTimeSeconds() + (g_config.audio_offset_ms / 1000.0);
            session.update(dt);
            highway.update(dt);

            // Check pause
            if (buttons_down & g_config.buttons[ACTION_PAUSE]) {
                audio.togglePause();
                state = STATE_PAUSE;
                pause_idx = 0;
            }

            // Star power activation
            if (buttons_down & g_config.buttons[ACTION_STAR_POWER]) {
                session.activateStarPower();
            }

            // Read held frets according to button bindings
            uint32_t held_frets_mask = 0;
            for (int lane = 0; lane < 5; ++lane) {
                if (buttons_held & g_config.buttons[ACTION_FRET_GREEN + lane]) {
                    held_frets_mask |= (1u << lane);
                }
            }

            // Note hitting and sustain logic
            if (g_config.autoplay) {
                // Autoplay mode
                for (auto& n : current_active_track.notes) {
                    if (n.time_seconds > current_time + 0.1) break; // Future notes
                    if (!n.hit && !n.missed && n.time_seconds <= current_time) {
                        n.hit = true;
                        if (n.lane >= 0 && n.lane < 5) {
                            held_frets_mask |= (1u << n.lane);
                        }
                        session.onHit(n.lane, 0.0, 1);
                        highway.triggerHitFX(n.lane, session.streak);
                        g_renderer.triggerBackgroundHit(n.lane, 1.0f);
                        if (n.duration_seconds > 0.05) {
                            n.sustaining = true;
                            n.sustain_dropped = false;
                            n.sustain_scored_time = n.time_seconds;
                        }
                    }
                }

                // Autoplay sustain maintenance
                for (auto& n : current_active_track.notes) {
                    if (n.time_seconds > current_time + 0.1) break;
                    if (n.sustaining) {
                        double note_end = n.time_seconds + n.duration_seconds;
                        if (current_time <= note_end) {
                            if (n.lane >= 0 && n.lane < 5) {
                                held_frets_mask |= (1u << n.lane);
                            }
                            double dt_held = std::min(current_time, note_end) - n.sustain_scored_time;
                            if (dt_held > 0.0) {
                                session.addSustainScore(dt_held);
                                session.rock = std::min(1.0f, session.rock + static_cast<float>(dt_held * 0.02f));
                                n.sustain_scored_time = std::min(current_time, note_end);
                            }
                            highway.triggerSustainFX(n.lane, session.star_active);
                        } else {
                            n.sustaining = false;
                        }
                    }
                }
            } else {
                // Manual Play
                bool strum_hit = !g_config.strum_required;
                if (buttons_down & (g_config.buttons[ACTION_STRUM_UP] | g_config.buttons[ACTION_STRUM_DOWN])) {
                    strum_hit = true;
                }

                // Fret press detection for tap mode
                uint32_t frets_pressed_now = 0;
                for (int lane = 0; lane < 5; ++lane) {
                    if (buttons_down & g_config.buttons[ACTION_FRET_GREEN + lane]) {
                        frets_pressed_now |= (1u << lane);
                    }
                }

                if (!g_config.strum_required && frets_pressed_now != 0) {
                    strum_hit = true;
                }

                if (strum_hit) {
                    bool note_hit_this_strum = false;
                    for (auto& n : current_active_track.notes) {
                        if (n.hit || n.missed) continue;
                        double diff = n.time_seconds - current_time;
                        if (diff > HIT_WINDOW) break; // Notes are too far in the future

                        if (std::abs(diff) <= HIT_WINDOW) {
                            if (n.lane == LANE_OPEN || (held_frets_mask & (1u << n.lane))) {
                                n.hit = true;
                                note_hit_this_strum = true;
                                session.onHit(n.lane, diff, 1);
                                highway.triggerHitFX(n.lane, session.streak);
                                g_renderer.triggerBackgroundHit(n.lane, 1.0f);
                                if (n.duration_seconds > 0.05) {
                                    n.sustaining = true;
                                    n.sustain_dropped = false;
                                    n.sustain_scored_time = n.time_seconds;
                                }
                                break;
                            }
                        }
                    }

                    if (!note_hit_this_strum && g_config.strum_required) {
                        session.onOverstrum();
                    }
                }

                // Sustain processing for manual play
                for (auto& n : current_active_track.notes) {
                    if (n.time_seconds > current_time + 0.1) break;
                    if (n.sustaining) {
                        double note_end = n.time_seconds + n.duration_seconds;
                        bool is_fret_held = (n.lane == LANE_OPEN) || ((held_frets_mask & (1u << n.lane)) != 0);

                        if (is_fret_held && current_time <= note_end) {
                            double dt_held = std::min(current_time, note_end) - n.sustain_scored_time;
                            if (dt_held > 0.0) {
                                session.addSustainScore(dt_held);
                                session.rock = std::min(1.0f, session.rock + static_cast<float>(dt_held * 0.02f));
                                n.sustain_scored_time = std::min(current_time, note_end);
                            }
                            highway.triggerSustainFX(n.lane, session.star_active);
                        } else if (!is_fret_held && current_time < note_end) {
                            // Player let go early
                            n.sustaining = false;
                            n.sustain_dropped = true;
                        } else {
                            // Reached end naturally
                            n.sustaining = false;
                        }
                    }
                }

                // Check for missed notes
                for (auto& n : current_active_track.notes) {
                    if (n.time_seconds > current_time) break; // Future notes cannot be missed yet
                    if (!n.hit && !n.missed && (current_time - n.time_seconds) > HIT_WINDOW) {
                        n.missed = true;
                        n.sustaining = false;
                        n.sustain_dropped = true;
                        session.onMiss();
                    }
                }
            }

            // Check song failure or completion
            if (session.failed) {
                audio.pause();
                state = STATE_RESULTS;
            } else {
                double last_note_end = 0.0;
                bool all_notes_finished = true;
                if (!current_active_track.notes.empty()) {
                    const auto& last = current_active_track.notes.back();
                    last_note_end = last.time_seconds + last.duration_seconds;
                    if (current_time < last_note_end + 1.2) {
                        all_notes_finished = false;
                    }
                }

                bool song_completed = false;
                if (all_notes_finished) {
                    if (audio.isLoaded()) {
                        if (audio.isEnded()) {
                            song_completed = true;
                        } else if (audio.getTotalDurationSeconds() > 0.0 && current_time >= (audio.getTotalDurationSeconds() - 0.2)) {
                            song_completed = true;
                        } else if (current_chart_song.total_duration_seconds > 0.0 && current_time >= current_chart_song.total_duration_seconds) {
                            song_completed = true;
                        } else if (last_note_end > 0.0 && current_time >= last_note_end + 3.5) {
                            if (audio.getTotalDurationSeconds() <= 0.0 || current_time >= audio.getTotalDurationSeconds()) {
                                song_completed = true;
                            }
                        }
                    } else {
                        if (current_chart_song.total_duration_seconds > 0.0 && current_time >= current_chart_song.total_duration_seconds) {
                            song_completed = true;
                        } else if (last_note_end > 0.0 && current_time >= last_note_end + 2.5) {
                            song_completed = true;
                        }
                    }
                }

                if (song_completed) {
                    audio.pause();
                    const SongItem* song = g_song_manager.getSong(current_song_idx);
                    if (song && !g_config.autoplay) {
                        uint32_t prev_best = g_song_manager.getBestScore(song->id, current_diff_name);
                        if (session.score > prev_best) {
                            is_new_best = true;
                            g_song_manager.setBestScore(song->id, current_diff_name, session.score);
                        }
                    }
                    state = STATE_RESULTS;
                }
            }

            g_renderer.beginFrame();
            highway.render(current_chart_song, current_active_track, current_time,
                           g_config.getTravelTime(), session, held_frets_mask, false);
            g_renderer.endFrame();
        }
        else if (state == STATE_PAUSE) {
            if (buttons_down & PSP_CTRL_UP) {
                pause_idx = (pause_idx - 1 + 3) % 3;
            }
            if (buttons_down & PSP_CTRL_DOWN) {
                pause_idx = (pause_idx + 1) % 3;
            }

            if (buttons_down & (PSP_CTRL_START | PSP_CTRL_CIRCLE)) {
                audio.togglePause();
                state = STATE_GAMEPLAY;
            }

            if (buttons_down & PSP_CTRL_CROSS) {
                if (pause_idx == 0) {
                    // Resume
                    audio.togglePause();
                    state = STATE_GAMEPLAY;
                } else if (pause_idx == 1) {
                    // Restart
                    audio.seek(0.0);
                    for (auto& n : current_active_track.notes) {
                        n.hit = false;
                        n.missed = false;
                        n.sustaining = false;
                        n.sustain_dropped = false;
                        n.sustain_scored_time = 0.0;
                    }
                    session.reset();
                    audio.play();
                    state = STATE_GAMEPLAY;
                } else if (pause_idx == 2) {
                    // Quit to Songs (YES: Pause menu has Quit button!)
                    audio.shutdown();
                    state = STATE_SONG_SELECT;
                }
            }

            g_renderer.beginFrame();
            double cur_time = audio.getTimeSeconds();
            highway.render(current_chart_song, current_active_track, cur_time,
                           g_config.getTravelTime(), session, 0, true);
            MenuScreens::renderPause(pause_idx);
            g_renderer.endFrame();
        }
        else if (state == STATE_RESULTS) {
            const SongItem* song = g_song_manager.getSong(current_song_idx);

            if (buttons_down & PSP_CTRL_CROSS) {
                // Retry
                if (song) {
                    for (auto& n : current_active_track.notes) {
                        n.hit = false;
                        n.missed = false;
                        n.sustaining = false;
                        n.sustain_dropped = false;
                        n.sustain_scored_time = 0.0;
                    }
                    audio.seek(0.0);
                    audio.play();
                    session.reset();
                    is_new_best = false;
                    state = STATE_GAMEPLAY;
                }
            }
            if (buttons_down & PSP_CTRL_CIRCLE) {
                // Back to Songs
                audio.shutdown();
                state = STATE_SONG_SELECT;
            }

            g_renderer.beginFrame();
            if (song) {
                MenuScreens::renderResults(*song, current_diff_name, session, is_new_best);
            }
            g_renderer.endFrame();
        }
    }

    audio.shutdown();
    g_renderer.shutdown();

#ifdef __PSP__
    sceKernelExitGame();
#endif

    return 0;
}
