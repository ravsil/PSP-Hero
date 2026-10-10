#ifndef MENU_SCREENS_H
#define MENU_SCREENS_H

#include "ui_common.h"
#include "song_manager.h"
#include "gameplay.h"
#include "config.h"

class MenuScreens {
public:
    static void renderTitle(float time);

    // Main Menu: Quick Play, Songs, Options (NO QUIT BUTTON)
    static void renderMainMenu(int selected_index);

    // Song Select (Library): Scrollable song cards
    static void renderSongSelect(const std::vector<SongItem>& songs, size_t selected_index, int scroll_offset);

    // Difficulty Select: Filtered difficulties for current song
    static void renderDifficultySelect(const SongItem& song, int selected_diff);

    // Options Screen: Note speed, strum required, autoplay, calibration, fx, remapper
    static void renderOptions(int selected_opt);

    // Button Remapper Screen
    static void renderRemapper(int selected_action, bool is_listening);

    // In-game Pause Overlay (Resume, Restart, Quit to Songs)
    static void renderPause(int selected_index);

    // Results Screen (Song Complete/Failed, Stars, Score, Accuracy)
    static void renderResults(const SongItem& song, const std::string& diff_name, const GameSession& session, bool is_new_best);

    // Texture cache for album covers
    static void loadSongCover(const SongItem& song);
    static void unloadSongCover();

private:
    static Texture s_album_tex;
    static std::string s_loaded_album_path;
};

#endif // MENU_SCREENS_H
