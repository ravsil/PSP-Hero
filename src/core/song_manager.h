#ifndef SONG_MANAGER_H
#define SONG_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>

struct SongTrackInfo {
    std::string track_name;     // e.g. "ExpertSingle", "HardSingle"
    std::string difficulty;     // e.g. "Easy", "Medium", "Hard", "Expert"
    std::string instrument;     // e.g. "Single", "DoubleGuitar"
    int note_count;
};

struct SongItem {
    std::string id;             // Folder basename
    std::string folder_path;
    std::string chart_path;
    std::string ini_path;
    std::string audio_path;
    std::string album_path;

    // Metadata
    std::string title;
    std::string artist;
    std::string album;
    std::string charter;
    std::string genre;
    std::string year;
    double bpm;
    double preview_start;
    double duration_seconds;

    // Available difficulties
    std::vector<SongTrackInfo> tracks;

    // High scores (difficulty name -> best score)
    std::map<std::string, uint32_t> best_scores;

    bool hasDifficulty(const std::string& diff) const;
    const SongTrackInfo* getTrackByDifficulty(const std::string& diff) const;
};

class SongManager {
public:
    SongManager();

    // Scan song directory and populate catalog
    bool scanSongs(const std::string& base_path = "song");

    // Catalog access
    size_t getSongCount() const { return m_songs.size(); }
    const SongItem* getSong(size_t index) const;
    const std::vector<SongItem>& getSongs() const { return m_songs; }

    // High score persistence
    uint32_t getBestScore(const std::string& song_id, const std::string& diff) const;
    void setBestScore(const std::string& song_id, const std::string& diff, uint32_t score);
    void saveScores(const std::string& filepath = "scores.dat");
    void loadScores(const std::string& filepath = "scores.dat");

private:
    std::vector<SongItem> m_songs;
    std::map<std::string, uint32_t> m_scores; // "song_id:diff" -> score

    void parseIniFile(const std::string& ini_path, SongItem& item);
    void inspectChartFile(const std::string& chart_path, SongItem& item);
};

// Global instance
extern SongManager g_song_manager;

#endif // SONG_MANAGER_H

