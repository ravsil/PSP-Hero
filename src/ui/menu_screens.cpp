#include "menu_screens.h"
#include <cstdio>
#include <cmath>
#include <algorithm>

#ifdef __PSP__
#include <psprtc.h>
static double getMenuTimeSeconds() {
    uint64_t tick;
    sceRtcGetCurrentTick(&tick);
    uint32_t freq = sceRtcGetTickResolution();
    return static_cast<double>(tick) / freq;
}
#else
#include <chrono>
static double getMenuTimeSeconds() {
    static auto start_time = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - start_time).count();
}
#endif

Texture MenuScreens::s_album_tex = { 0, 0, 0, nullptr, false };
std::string MenuScreens::s_loaded_album_path = "";
std::map<std::string, Texture> MenuScreens::s_cover_cache;
size_t MenuScreens::s_last_selected_idx = (size_t)-1;
double MenuScreens::s_selection_time = 0.0;

std::string truncateText(const std::string& str, size_t max_len) {
    if (str.length() <= max_len) return str;
    if (max_len <= 3) return str.substr(0, max_len);
    return str.substr(0, max_len - 3) + "...";
}

void drawHeaderBar(const std::string& title, const std::string& subtitle) {
    // Back arrow chevron ‹
    g_renderer.drawLine(24, 22, 17, 28, COLOR_GRAY);
    g_renderer.drawLine(17, 28, 24, 34, COLOR_GRAY);

    g_renderer.drawText(34, 18, title, COLOR_WHITE, 2);
    if (!subtitle.empty()) {
        g_renderer.drawText(34, 36, truncateText(subtitle, 50), COLOR_GRAY, 1);
    }
}

void MenuScreens::loadSongCover(const SongItem& song) {
    if (song.album_path.empty()) return;

    auto it = s_cover_cache.find(song.album_path);
    if (it != s_cover_cache.end()) {
        s_album_tex = it->second;
        s_loaded_album_path = song.album_path;
        return;
    }

    Texture tex = g_renderer.loadTexture(song.album_path, 36, 36);
    s_cover_cache[song.album_path] = tex;
    s_album_tex = tex;
    s_loaded_album_path = song.album_path;
}

void MenuScreens::unloadSongCover() {
    s_album_tex = { 0, 0, 0, nullptr, false };
    s_loaded_album_path = "";
}

void MenuScreens::clearCoverCache() {
    for (auto& pair : s_cover_cache) {
        if (pair.second.is_valid) {
            g_renderer.freeTexture(pair.second);
        }
    }
    s_cover_cache.clear();
    s_album_tex = { 0, 0, 0, nullptr, false };
    s_loaded_album_path = "";
    s_last_selected_idx = (size_t)-1;
}

void MenuScreens::renderTitle(float time) {
    g_renderer.renderBackground(false);

    // Centered Title Logo
    g_renderer.drawLogo(240, 96, 3);

    // Subtitle
    g_renderer.drawTextCentered(240, 148, "PORTABLE RHYTHM ENGINE", COLOR_GRAY, 1);

    // Pulsing prompt
    float pulse = std::sin(time * 3.5f) * 0.5f + 0.5f;
    if (pulse > 0.25f) {
        g_renderer.drawTextCentered(240, 206, "PRESS START OR CROSS TO BEGIN", COLOR_WHITE, 1);
    }

    // Version label
    g_renderer.drawTextRight(470, 258, "PSP Hero v1.0", COLOR_DIM, 1);
}

void MenuScreens::renderMainMenu(int selected_index) {
    g_renderer.renderBackground(false);

    // Logo at top
    g_renderer.drawLogo(240, 48, 2);

    // Menu items (CRITICAL: NO QUIT BUTTON)
    static const char* items[3] = {
        "Quick Play",
        "Song Select",
        "Options"
    };

    for (int i = 0; i < 3; ++i) {
        int y = 118 + i * 36;
        bool sel = (selected_index == i);

        int bx = 140, bw = 200, bh = 28;
        if (sel) {
            g_renderer.drawRoundedRect(bx, y, bw, bh, 4, COLOR_RGBA(255, 255, 255, 28), COLOR_RGBA(255, 255, 255, 120));
            g_renderer.drawPick(bx - 12, y + bh / 2 - 2, Renderer::getLaneColor(i));
        } else {
            g_renderer.drawRoundedRect(bx, y, bw, bh, 4, COLOR_PANEL_BG, COLOR_PANEL_BORDER);
        }

        unsigned int text_col = sel ? COLOR_WHITE : COLOR_GRAY;
        g_renderer.drawTextCentered(240, y + 9, items[i], text_col, 1);
    }

    // Bottom hints
    int hx = 16;
    hx += g_renderer.drawButtonHint(hx, 252, ICON_CROSS, "Select");
    hx += g_renderer.drawButtonHint(hx + 8, 252, ICON_DPAD, "Move");
}

void MenuScreens::renderSongSelect(const std::vector<SongItem>& songs, size_t selected_index, int scroll_offset) {
    g_renderer.renderBackground(false);

    drawHeaderBar("Song Library", "Select a track to play");

    if (songs.empty()) {
        g_renderer.drawTextCentered(240, 130, "No songs found in 'song/' folder!", COLOR_LANE_RED, 1);
        g_renderer.drawTextCentered(240, 146, "Place song folders with notes.chart & audio", COLOR_GRAY, 1);
        g_renderer.drawButtonHint(16, 252, ICON_CIRCLE, "Back");
        return;
    }

    // Debounced cover loading: track time resting on currently selected song
    double cur_time = getMenuTimeSeconds();
    if (selected_index != s_last_selected_idx) {
        s_last_selected_idx = selected_index;
        s_selection_time = cur_time;
    }

    if (selected_index < songs.size()) {
        const auto& cur_song = songs[selected_index];
        if (!cur_song.album_path.empty()) {
            auto it = s_cover_cache.find(cur_song.album_path);
            if (it == s_cover_cache.end()) {
                // If the user rests on this song for at least 80ms, load and cache it
                if ((cur_time - s_selection_time) >= 0.08) {
                    loadSongCover(cur_song);
                }
            } else {
                s_album_tex = it->second;
                s_loaded_album_path = cur_song.album_path;
            }
        }
    }

    // Display 4 cards
    const int cards_per_page = 4;
    for (int i = 0; i < cards_per_page; ++i) {
        size_t idx = scroll_offset + i;
        if (idx >= songs.size()) break;

        const auto& song = songs[idx];
        int y = 56 + i * 48;
        bool sel = (idx == selected_index);

        int bx = 32, bw = 416, bh = 42;
        unsigned int border = sel ? Renderer::getLaneColor(idx % 5) : COLOR_PANEL_BORDER;
        unsigned int bg = sel ? COLOR_RGBA(255, 255, 255, 30) : COLOR_PANEL_BG;

        g_renderer.drawRoundedRect(bx, y, bw, bh, 4, bg, border);

        if (sel) {
            g_renderer.drawPick(bx - 10, y + bh / 2 - 2, Renderer::getLaneColor(idx % 5));
        }

        // Album artwork or colored icon
        int icon_x = bx + 4, icon_y = y + 3, icon_sz = 36;
        bool drew_cover = false;
        if (!song.album_path.empty()) {
            auto it = s_cover_cache.find(song.album_path);
            if (it != s_cover_cache.end() && it->second.is_valid) {
                g_renderer.drawTexture(it->second, icon_x, icon_y, icon_sz, icon_sz);
                drew_cover = true;
            }
        }

        if (!drew_cover) {
            g_renderer.drawRoundedRect(icon_x, icon_y, icon_sz, icon_sz, 2, Renderer::getLaneColor(idx % 5), 0);
            char initial[2] = { song.title.empty() ? '?' : song.title[0], '\0' };
            g_renderer.drawTextCentered(icon_x + icon_sz / 2, icon_y + 11, initial, COLOR_WHITE, 2);
        }

        // Title & Artist
        int text_x = icon_x + icon_sz + 10;
        unsigned int title_col = sel ? COLOR_WHITE : COLOR_RGBA(220, 215, 235, 255);
        g_renderer.drawText(text_x, y + 8, truncateText(song.title, 26), title_col, 1);
        g_renderer.drawText(text_x, y + 23, truncateText(song.artist, 26), COLOR_GRAY, 1);

        // BPM & High score on right
        char bpm_buf[32];
        std::snprintf(bpm_buf, sizeof(bpm_buf), "%.0f BPM", song.bpm);
        g_renderer.drawTextRight(bx + bw - 12, y + 8, bpm_buf, COLOR_GRAY, 1);

        // Best score
        uint32_t best = 0;
        for (const auto& pair : song.best_scores) {
            if (pair.second > best) best = pair.second;
        }
        if (best > 0) {
            char best_buf[32];
            std::snprintf(best_buf, sizeof(best_buf), "Best: %lu", static_cast<unsigned long>(best));
            g_renderer.drawTextRight(bx + bw - 12, y + 23, best_buf, COLOR_GOLD, 1);
        } else {
            g_renderer.drawTextRight(bx + bw - 12, y + 23, "No score", COLOR_DIM, 1);
        }
    }

    // Bottom hints & Counter
    int hx = 16;
    hx += g_renderer.drawButtonHint(hx, 252, ICON_CROSS, "Select");
    hx += g_renderer.drawButtonHint(hx + 8, 252, ICON_CIRCLE, "Back");
    hx += g_renderer.drawButtonHint(hx + 8, 252, ICON_DPAD, "Move / Page");

    char count_buf[32];
    std::snprintf(count_buf, sizeof(count_buf), "%zu / %zu", selected_index + 1, songs.size());
    g_renderer.drawTextRight(464, 254, count_buf, COLOR_GRAY, 1);
}

void MenuScreens::renderDifficultySelect(const SongItem& song, int selected_diff) {
    g_renderer.renderBackground(false);

    std::string diff_sub = song.title + (song.artist.empty() ? "" : (" - " + song.artist));
    drawHeaderBar("Difficulty", truncateText(diff_sub, 48));

    std::vector<std::string> diffs = song.getAvailableDifficulties();
    int count = static_cast<int>(diffs.size());
    if (count == 0) return;

    int item_h = 32;
    int gap = (count <= 2) ? 14 : ((count == 3) ? 10 : 8);
    int total_h = count * item_h + (count - 1) * gap;
    int start_y = 56 + (180 - total_h) / 2;

    for (int i = 0; i < count; ++i) {
        int y = start_y + i * (item_h + gap);
        bool sel = (selected_diff == i);
        const std::string& diff_name = diffs[i];

        int bx = 60, bw = 360, bh = item_h;
        unsigned int col = COLOR_WHITE;
        if (diff_name == "Easy") col = Renderer::getLaneColor(0);
        else if (diff_name == "Medium") col = Renderer::getLaneColor(1);
        else if (diff_name == "Hard") col = Renderer::getLaneColor(2);
        else if (diff_name == "Expert") col = Renderer::getLaneColor(3);
        else col = Renderer::getLaneColor(i % 5);

        unsigned int bg = sel ? COLOR_RGBA(255, 255, 255, 28) : COLOR_PANEL_BG;
        unsigned int border = sel ? col : COLOR_PANEL_BORDER;

        g_renderer.drawRoundedRect(bx, y, bw, bh, 4, bg, border);

        if (sel) {
            g_renderer.drawPick(bx - 12, y + bh / 2 - 2, col);
        }

        unsigned int name_col = sel ? COLOR_WHITE : COLOR_GRAY;
        g_renderer.drawText(bx + 16, y + (bh - 8) / 2, diff_name, name_col, 1);

        // Note count if track exists
        const SongTrackInfo* track = song.getTrackByDifficulty(diff_name);
        if (track && track->note_count > 0) {
            char note_buf[32];
            std::snprintf(note_buf, sizeof(note_buf), "%d notes", track->note_count);
            g_renderer.drawTextRight(bx + bw - 16, y + (bh - 8) / 2, note_buf, COLOR_GRAY, 1);
        }
    }

    // Bottom hints
    int hx = 16;
    hx += g_renderer.drawButtonHint(hx, 252, ICON_CROSS, "Play");
    hx += g_renderer.drawButtonHint(hx + 8, 252, ICON_CIRCLE, "Back");
    if (count > 1) {
        hx += g_renderer.drawButtonHint(hx + 8, 252, ICON_DPAD, "Move");
    }
}

void MenuScreens::renderOptions(int selected_opt) {
    g_renderer.renderBackground(false);

    drawHeaderBar("Options", "Adjust gameplay & controls");

    static const char* opt_titles[6] = {
        "Button Remapper",
        "Note Speed",
        "Strum Required",
        "Autoplay",
        "Audio Offset",
        "Background FX"
    };

    for (int i = 0; i < 6; ++i) {
        int y = 56 + i * 31;
        bool sel = (selected_opt == i);

        int bx = 50, bw = 380, bh = 26;
        unsigned int bg = sel ? COLOR_RGBA(255, 255, 255, 28) : COLOR_PANEL_BG;
        unsigned int border = sel ? Renderer::getLaneColor(i % 5) : COLOR_PANEL_BORDER;

        g_renderer.drawRoundedRect(bx, y, bw, bh, 3, bg, border);

        if (sel) {
            g_renderer.drawPick(bx - 10, y + bh / 2 - 2, Renderer::getLaneColor(i % 5));
        }

        unsigned int tcol = sel ? COLOR_WHITE : COLOR_GRAY;
        g_renderer.drawText(bx + 14, y + 8, opt_titles[i], tcol, 1);

        // Value on right
        int val_x = bx + bw - 20;
        switch (i) {
            case 0:
                g_renderer.drawTextRight(val_x, y + 8, "Configure >", COLOR_CYAN, 1);
                break;
            case 1: {
                char speed_buf[16];
                std::snprintf(speed_buf, sizeof(speed_buf), "< %d >", g_config.note_speed);
                g_renderer.drawTextRight(val_x, y + 8, speed_buf, COLOR_WHITE, 1);
                break;
            }
            case 2:
                g_renderer.drawTextRight(val_x, y + 8, g_config.strum_required ? "< On >" : "< Off >", COLOR_WHITE, 1);
                break;
            case 3:
                g_renderer.drawTextRight(val_x, y + 8, g_config.autoplay ? "< On >" : "< Off >", COLOR_WHITE, 1);
                break;
            case 4: {
                char off_buf[32];
                std::snprintf(off_buf, sizeof(off_buf), "< %+d ms >", g_config.audio_offset_ms);
                g_renderer.drawTextRight(val_x, y + 8, off_buf, COLOR_WHITE, 1);
                break;
            }
            case 5:
                g_renderer.drawTextRight(val_x, y + 8, g_config.background_fx ? "< Full >" : "< Low >", COLOR_WHITE, 1);
                break;
        }
    }

    // Explanation hint
    static const char* hints[6] = {
        "Customize PSP button bindings for frets and strumming",
        "Higher speeds decrease note arrival time for tighter reading",
        "When Off, tapping frets hits notes directly without strumming",
        "Demonstration mode automatically hits all notes with perfect timing",
        "Compensate for display latency or audio sync offset",
        "Toggle reactive equalizers, glowing rings, and particle bursts"
    };
    g_renderer.drawTextCentered(240, 238, hints[selected_opt], COLOR_GRAY, 1);

    // Bottom hints
    int hx = 16;
    hx += g_renderer.drawButtonHint(hx, 254, ICON_CROSS, "Select / Change");
    hx += g_renderer.drawButtonHint(hx + 8, 254, ICON_CIRCLE, "Back");
    hx += g_renderer.drawButtonHint(hx + 8, 254, ICON_DPAD, "Adjust");
}

void MenuScreens::renderRemapper(int selected_action, bool is_listening) {
    g_renderer.renderBackground(false);

    drawHeaderBar("Button Remapper", is_listening ? "Press a PSP button to assign..." : "Select an action to remap");

    const int total_rows = ACTION_COUNT + 1; // actions + "Reset to Default"
    for (int i = 0; i < total_rows; ++i) {
        int y = 52 + i * 19;
        bool sel = (selected_action == i);

        int bx = 50, bw = 380, bh = 17;
        unsigned int bg = sel ? COLOR_RGBA(255, 255, 255, 25) : COLOR_PANEL_BG;
        unsigned int border = sel ? COLOR_GOLD : COLOR_PANEL_BORDER;

        g_renderer.drawRoundedRect(bx, y, bw, bh, 2, bg, border);

        if (sel) {
            g_renderer.drawPick(bx - 8, y + bh / 2 - 2, COLOR_GOLD);
        }

        if (i < ACTION_COUNT) {
            ActionType act = static_cast<ActionType>(i);
            unsigned int lane_col = (i < 5) ? Renderer::getLaneColor(i) : COLOR_GRAY;
            g_renderer.drawText(bx + 12, y + 4, GameConfig::getActionName(act), lane_col, 1);

            if (sel && is_listening) {
                g_renderer.drawTextRight(bx + bw - 14, y + 4, "[ PRESS BUTTON... ]", COLOR_GOLD, 1);
            } else {
                std::string btn_name = GameConfig::getButtonName(g_config.buttons[i]);
                g_renderer.drawTextRight(bx + bw - 14, y + 4, btn_name, COLOR_WHITE, 1);
            }
        } else {
            // Reset to Default
            unsigned int reset_col = sel ? COLOR_LANE_ORANGE : COLOR_GRAY;
            g_renderer.drawText(bx + 12, y + 4, "Reset to Default Layout", reset_col, 1);
        }
    }

    // Bottom hints
    int hx = 16;
    hx += g_renderer.drawButtonHint(hx, 254, ICON_CROSS, is_listening ? "Cancel" : "Remap");
    hx += g_renderer.drawButtonHint(hx + 8, 254, ICON_CIRCLE, "Back");
    hx += g_renderer.drawButtonHint(hx + 8, 254, ICON_DPAD, "Move");
}

void MenuScreens::renderPause(int selected_index) {
    // Dimmed overlay
    g_renderer.drawRect(0, 0, 480, 272, COLOR_RGBA(0, 0, 0, 160));

    g_renderer.drawTextCentered(240, 70, "PAUSED", COLOR_WHITE, 3);

    // Pause menu items (YES: Pause menu HAS Quit button as specified)
    static const char* items[3] = {
        "Resume",
        "Restart",
        "Quit to Songs"
    };

    for (int i = 0; i < 3; ++i) {
        int y = 122 + i * 34;
        bool sel = (selected_index == i);

        int bx = 140, bw = 200, bh = 26;
        unsigned int bg = sel ? COLOR_RGBA(255, 255, 255, 30) : COLOR_PANEL_BG;
        unsigned int border = sel ? Renderer::getLaneColor(i) : COLOR_PANEL_BORDER;

        g_renderer.drawRoundedRect(bx, y, bw, bh, 4, bg, border);

        if (sel) {
            g_renderer.drawPick(bx - 12, y + bh / 2 - 2, Renderer::getLaneColor(i));
        }

        unsigned int tcol = sel ? COLOR_WHITE : COLOR_GRAY;
        g_renderer.drawTextCentered(240, y + 8, items[i], tcol, 1);
    }

    // Bottom hints
    int hx = 16;
    hx += g_renderer.drawButtonHint(hx, 252, ICON_CROSS, "Select");
    hx += g_renderer.drawButtonHint(hx + 8, 252, ICON_DPAD, "Move");
}

void MenuScreens::renderCountdown(float countdown_timer) {
    // Subtle backdrop dim so the countdown pops over the highway
    g_renderer.drawRect(0, 0, 480, 272, COLOR_RGBA(0, 0, 0, 110));

    // Determine current number (3, 2, or 1)
    int num = static_cast<int>(std::ceil(countdown_timer));
    if (num < 1) num = 1;
    if (num > 3) num = 3;

    // Fractional progress in current second (1.0 down to 0.0)
    float frac = countdown_timer - std::floor(countdown_timer);
    if (frac <= 0.001f) frac = 1.0f;

    // Stylish glassmorphic badge in the center of the screen
    int bw = 96, bh = 84;
    int bx = 240 - bw / 2;
    int by = 136 - bh / 2 - 4;
    g_renderer.drawRoundedRect(bx, by, bw, bh, 8, COLOR_RGBA(12, 10, 24, 230), COLOR_RGBA(255, 210, 63, 190));

    // Centered countdown ring around the number
    int ring_cy = by + 30;
    int ring_r = 21;
    // Dim background track
    g_renderer.drawCircle(240, ring_cy, ring_r, COLOR_RGBA(255, 210, 63, 40), false);
    // Active progress arc sweeping clockwise from top (-PI/2)
    g_renderer.drawArc(240, ring_cy, ring_r, -1.5707963f, -1.5707963f + 6.2831853f * frac, COLOR_GOLD, 2);

    // Number text (Scale 3) centered inside ring
    std::string num_str = std::to_string(num);
    g_renderer.drawTextCentered(240, ring_cy - 11, num_str, COLOR_WHITE, 3);

    // Subtitle
    g_renderer.drawTextCentered(240, by + bh - 18, "GET READY", COLOR_RGBA(220, 215, 240, 255), 1);
}

void MenuScreens::renderResults(const SongItem& song, const std::string& diff_name, const GameSession& session, bool is_new_best) {
    g_renderer.renderBackground(false);

    // Results container card (centered on 480x272 PSP screen)
    int bx = 36, by = 14, bw = 408, bh = 236;
    g_renderer.drawRoundedRect(bx, by, bw, bh, 6, COLOR_PANEL_BG, COLOR_PANEL_BORDER);

    // Header
    if (session.failed) {
        g_renderer.drawTextCentered(240, by + 12, "SONG FAILED", COLOR_LANE_RED, 2);
    } else {
        g_renderer.drawTextCentered(240, by + 12, "SONG COMPLETE", COLOR_WHITE, 2);
    }

    // Subtitle: Song Title on line 1, Artist & Difficulty on line 2
    std::string title_str = truncateText(song.title, 46);
    g_renderer.drawTextCentered(240, by + 34, title_str, COLOR_WHITE, 1);

    std::string info_str;
    if (!song.artist.empty()) {
        info_str = truncateText(song.artist, 28) + "  -  " + diff_name;
    } else {
        info_str = diff_name;
    }
    g_renderer.drawTextCentered(240, by + 46, info_str, COLOR_GRAY, 1);

    // 5 Stars
    int stars = session.calculateStars();
    for (int i = 0; i < 5; ++i) {
        g_renderer.drawStar(240 - 56 + i * 28, by + 62, 10, i < stars);
    }

    // Score
    char score_buf[32];
    std::snprintf(score_buf, sizeof(score_buf), "%lu", static_cast<unsigned long>(session.score));
    g_renderer.drawTextCentered(240, by + 90, score_buf, COLOR_WHITE, 3);

    if (is_new_best && !g_config.autoplay) {
        g_renderer.drawTextCentered(240, by + 118, "NEW BEST SCORE!", COLOR_GOLD, 1);
    } else if (g_config.autoplay) {
        g_renderer.drawTextCentered(240, by + 118, "AUTOPLAY - NO SCORE SAVED", COLOR_CYAN, 1);
    }

    // Stats Grid - 3 distinct, styled sub-panels
    int panel_y = by + 134;
    int panel_w = 118, panel_h = 44;

    // Panel 1: Accuracy
    int p1_x = 48;
    g_renderer.drawRoundedRect(p1_x, panel_y, panel_w, panel_h, 4,
                               COLOR_RGBA(255, 255, 255, 12), COLOR_PANEL_BORDER);
    char acc_buf[32];
    std::snprintf(acc_buf, sizeof(acc_buf), "%d%%", static_cast<int>(session.accuracy * 100.0));
    unsigned int acc_col = (session.accuracy >= 0.999) ? COLOR_GOLD :
                           ((session.accuracy >= 0.8) ? COLOR_WHITE : COLOR_GRAY);
    g_renderer.drawTextCentered(p1_x + panel_w / 2, panel_y + 6, acc_buf, acc_col, 2);
    g_renderer.drawTextCentered(p1_x + panel_w / 2, panel_y + 26, "Accuracy", COLOR_GRAY, 1);

    // Panel 2: Best Streak
    int p2_x = 181;
    g_renderer.drawRoundedRect(p2_x, panel_y, panel_w, panel_h, 4,
                               COLOR_RGBA(255, 255, 255, 12), COLOR_PANEL_BORDER);
    char streak_buf[32];
    std::snprintf(streak_buf, sizeof(streak_buf), "%lu", static_cast<unsigned long>(session.max_streak));
    g_renderer.drawTextCentered(p2_x + panel_w / 2, panel_y + 6, streak_buf, COLOR_WHITE, 2);
    g_renderer.drawTextCentered(p2_x + panel_w / 2, panel_y + 26, "Best Streak", COLOR_GRAY, 1);

    // Panel 3: Notes Hit
    int p3_x = 314;
    g_renderer.drawRoundedRect(p3_x, panel_y, panel_w, panel_h, 4,
                               COLOR_RGBA(255, 255, 255, 12), COLOR_PANEL_BORDER);
    char hit_buf[32];
    std::snprintf(hit_buf, sizeof(hit_buf), "%lu", static_cast<unsigned long>(session.notes_hit));
    unsigned long total_notes = static_cast<unsigned long>(session.notes_hit + session.notes_missed);
    unsigned int hit_col = (session.notes_missed == 0 && session.notes_hit > 0) ? COLOR_GOLD : COLOR_WHITE;
    g_renderer.drawTextCentered(p3_x + panel_w / 2, panel_y + 6, hit_buf, hit_col, 2);

    char total_buf[32];
    if (total_notes > 0) {
        std::snprintf(total_buf, sizeof(total_buf), "/ %lu Notes", total_notes);
    } else {
        std::snprintf(total_buf, sizeof(total_buf), "Notes Hit");
    }
    g_renderer.drawTextCentered(p3_x + panel_w / 2, panel_y + 26, total_buf, COLOR_GRAY, 1);

    // Bottom hints
    g_renderer.drawButtonHint(160, 254, ICON_CROSS, "Retry");
    g_renderer.drawButtonHint(256, 254, ICON_CIRCLE, "Songs");
}
