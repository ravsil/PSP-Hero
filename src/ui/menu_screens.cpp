#include "menu_screens.h"
#include <cstdio>
#include <cmath>
#include <algorithm>

Texture MenuScreens::s_album_tex = { 0, 0, 0, nullptr, false };
std::string MenuScreens::s_loaded_album_path = "";

void drawHeaderBar(const std::string& title, const std::string& subtitle) {
    // Back arrow chevron ‹
    g_renderer.drawLine(24, 22, 17, 28, COLOR_GRAY);
    g_renderer.drawLine(17, 28, 24, 34, COLOR_GRAY);

    g_renderer.drawText(34, 18, title, COLOR_WHITE, 2);
    if (!subtitle.empty()) {
        g_renderer.drawText(34, 36, subtitle, COLOR_GRAY, 1);
    }
}

void MenuScreens::loadSongCover(const SongItem& song) {
    if (song.album_path == s_loaded_album_path) {
        return;
    }
    unloadSongCover();
    s_loaded_album_path = song.album_path;

    if (!song.album_path.empty()) {
        s_album_tex = g_renderer.loadTexture(song.album_path, 36, 36);
    }
}

void MenuScreens::unloadSongCover() {
    if (s_album_tex.is_valid) {
        g_renderer.freeTexture(s_album_tex);
    }
    s_loaded_album_path = "";
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
        if (sel) {
            loadSongCover(song);
        }

        if (sel && s_album_tex.is_valid && s_loaded_album_path == song.album_path) {
            g_renderer.drawTexture(s_album_tex, icon_x, icon_y, icon_sz, icon_sz);
        } else {
            g_renderer.drawRoundedRect(icon_x, icon_y, icon_sz, icon_sz, 2, Renderer::getLaneColor(idx % 5), 0);
            char initial[2] = { song.title.empty() ? '?' : song.title[0], '\0' };
            g_renderer.drawTextCentered(icon_x + icon_sz / 2, icon_y + 11, initial, COLOR_WHITE, 2);
        }

        // Title & Artist
        int text_x = icon_x + icon_sz + 10;
        unsigned int title_col = sel ? COLOR_WHITE : COLOR_RGBA(220, 215, 235, 255);
        g_renderer.drawText(text_x, y + 8, song.title, title_col, 1);
        g_renderer.drawText(text_x, y + 23, song.artist, COLOR_GRAY, 1);

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
    hx += g_renderer.drawButtonHint(hx + 8, 252, ICON_DPAD, "Move");

    char count_buf[32];
    std::snprintf(count_buf, sizeof(count_buf), "%zu / %zu", selected_index + 1, songs.size());
    g_renderer.drawTextRight(464, 254, count_buf, COLOR_GRAY, 1);
}

void MenuScreens::renderDifficultySelect(const SongItem& song, int selected_diff) {
    g_renderer.renderBackground(false);

    drawHeaderBar("Difficulty", song.title + " - " + song.artist);

    // List difficulties available in track
    static const char* all_diffs[4] = { "Easy", "Medium", "Hard", "Expert" };
    for (int i = 0; i < 4; ++i) {
        int y = 70 + i * 40;
        bool sel = (selected_diff == i);
        bool exists = song.hasDifficulty(all_diffs[i]);

        int bx = 60, bw = 360, bh = 32;
        unsigned int col = Renderer::getLaneColor(i);
        unsigned int bg = sel ? COLOR_RGBA(255, 255, 255, 28) : COLOR_PANEL_BG;
        unsigned int border = sel ? col : COLOR_PANEL_BORDER;

        g_renderer.drawRoundedRect(bx, y, bw, bh, 4, bg, border);

        if (sel) {
            g_renderer.drawPick(bx - 12, y + bh / 2 - 2, col);
        }

        unsigned int name_col = exists ? (sel ? COLOR_WHITE : COLOR_GRAY) : COLOR_DIM;
        g_renderer.drawText(bx + 16, y + 11, all_diffs[i], name_col, 1);

        // Note count if exists
        const SongTrackInfo* track = song.getTrackByDifficulty(all_diffs[i]);
        if (exists && track) {
            char note_buf[32];
            std::snprintf(note_buf, sizeof(note_buf), "%d notes", track->note_count);
            g_renderer.drawTextRight(bx + bw - 16, y + 11, note_buf, COLOR_GRAY, 1);
        } else {
            g_renderer.drawTextRight(bx + bw - 16, y + 11, "Not in chart", COLOR_DIM, 1);
        }
    }

    // Bottom hints
    int hx = 16;
    hx += g_renderer.drawButtonHint(hx, 252, ICON_CROSS, "Play");
    hx += g_renderer.drawButtonHint(hx + 8, 252, ICON_CIRCLE, "Back");
    hx += g_renderer.drawButtonHint(hx + 8, 252, ICON_DPAD, "Move");
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

void MenuScreens::renderResults(const SongItem& song, const std::string& diff_name, const GameSession& session, bool is_new_best) {
    g_renderer.renderBackground(false);

    // Results container card
    int bx = 70, by = 20, bw = 340, bh = 226;
    g_renderer.drawRoundedRect(bx, by, bw, bh, 6, COLOR_PANEL_BG, COLOR_PANEL_BORDER);

    // Header
    if (session.failed) {
        g_renderer.drawTextCentered(240, by + 16, "SONG FAILED", COLOR_LANE_RED, 2);
    } else {
        g_renderer.drawTextCentered(240, by + 16, "SONG COMPLETE", COLOR_WHITE, 2);
    }

    // Subtitle
    g_renderer.drawTextCentered(240, by + 36, song.title + " · " + diff_name, COLOR_GRAY, 1);

    // 5 Stars
    int stars = session.calculateStars();
    for (int i = 0; i < 5; ++i) {
        g_renderer.drawStar(240 - 56 + i * 28, by + 60, 10, i < stars);
    }

    // Score
    char score_buf[32];
    std::snprintf(score_buf, sizeof(score_buf), "%lu", static_cast<unsigned long>(session.score));
    g_renderer.drawTextCentered(240, by + 90, score_buf, COLOR_WHITE, 3);

    if (is_new_best) {
        g_renderer.drawTextCentered(240, by + 120, "NEW BEST SCORE!", COLOR_GOLD, 1);
    }

    // Stats Grid
    int y_stats = by + 144;
    char acc_buf[32];
    std::snprintf(acc_buf, sizeof(acc_buf), "%d%%", static_cast<int>(session.accuracy * 100.0));
    g_renderer.drawTextCentered(130, y_stats, acc_buf, COLOR_WHITE, 2);
    g_renderer.drawTextCentered(130, y_stats + 18, "Accuracy", COLOR_GRAY, 1);

    char streak_buf[32];
    std::snprintf(streak_buf, sizeof(streak_buf), "%lu", static_cast<unsigned long>(session.max_streak));
    g_renderer.drawTextCentered(240, y_stats, streak_buf, COLOR_WHITE, 2);
    g_renderer.drawTextCentered(240, y_stats + 18, "Best Streak", COLOR_GRAY, 1);

    char notes_buf[32];
    std::snprintf(notes_buf, sizeof(notes_buf), "%lu / %lu",
                  static_cast<unsigned long>(session.notes_hit),
                  static_cast<unsigned long>(session.notes_hit + session.notes_missed));
    g_renderer.drawTextCentered(350, y_stats, notes_buf, COLOR_WHITE, 2);
    g_renderer.drawTextCentered(350, y_stats + 18, "Notes Hit", COLOR_GRAY, 1);

    // Bottom hints
    g_renderer.drawButtonHint(160, 254, ICON_CROSS, "Retry");
    g_renderer.drawButtonHint(256, 254, ICON_CIRCLE, "Songs");
}
