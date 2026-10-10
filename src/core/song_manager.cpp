#include "song_manager.h"
#include "chart_parser.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <fstream>
#include <sstream>

#ifdef __PSP__
#include <pspkernel.h>
#endif

SongManager g_song_manager;

bool SongItem::hasDifficulty(const std::string& diff) const {
    for (const auto& t : tracks) {
        if (t.difficulty == diff) return true;
    }
    return false;
}

const SongTrackInfo* SongItem::getTrackByDifficulty(const std::string& diff) const {
    for (const auto& t : tracks) {
        if (t.difficulty == diff) return &t;
    }
    if (!tracks.empty()) return &tracks[0];
    return nullptr;
}

SongManager::SongManager() {
    loadScores();
}

const SongItem* SongManager::getSong(size_t index) const {
    if (index < m_songs.size()) {
        return &m_songs[index];
    }
    return nullptr;
}

static bool fileExists(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f) {
        std::fclose(f);
        return true;
    }
    return false;
}

static std::string findFirstExisting(const std::vector<std::string>& candidates) {
    for (const auto& c : candidates) {
        if (fileExists(c)) return c;
    }
    return "";
}

void SongManager::parseIniFile(const std::string& ini_path, SongItem& item) {
    std::ifstream file(ini_path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        // Trim
        while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) key.erase(0, 1);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        while (!val.empty() && (val.front() == ' ' || val.front() == '\t' || val.front() == '"')) val.erase(0, 1);
        while (!val.empty() && (val.back() == ' ' || val.back() == '\t' || val.back() == '"')) val.pop_back();

        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        if (key == "name") item.title = ChartParser::stripColorTags(val);
        else if (key == "artist") item.artist = ChartParser::stripColorTags(val);
        else if (key == "album") item.album = ChartParser::stripColorTags(val);
        else if (key == "charter") item.charter = ChartParser::stripColorTags(val);
        else if (key == "genre") item.genre = ChartParser::stripColorTags(val);
        else if (key == "year") item.year = val;
        else if (key == "song_length") {
            item.duration_seconds = std::strtod(val.c_str(), nullptr) / 1000.0;
        }
        else if (key == "preview_start_time") {
            item.preview_start = std::strtod(val.c_str(), nullptr) / 1000.0;
        }
    }
}

void SongManager::inspectChartFile(const std::string& chart_path, SongItem& item) {
    std::ifstream file(chart_path);
    if (!file.is_open()) return;

    std::string line;
    std::string current_section;
    SongTrackInfo current_track;
    bool in_track = false;
    int note_count = 0;

    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) line.erase(0, 1);

        if (line.empty()) continue;

        if (line.front() == '[' && line.back() == ']') {
            if (in_track && !current_track.track_name.empty()) {
                current_track.note_count = note_count;
                item.tracks.push_back(current_track);
                in_track = false;
            }

            current_section = line.substr(1, line.length() - 2);
            note_count = 0;

            if (current_section != "Song" && current_section != "SyncTrack" && current_section != "Events") {
                in_track = true;
                current_track.track_name = current_section;
                current_track.note_count = 0;

                // Derive difficulty & instrument
                if (current_section.rfind("Easy", 0) == 0) {
                    current_track.difficulty = "Easy";
                    current_track.instrument = current_section.substr(4);
                } else if (current_section.rfind("Medium", 0) == 0) {
                    current_track.difficulty = "Medium";
                    current_track.instrument = current_section.substr(6);
                } else if (current_section.rfind("Hard", 0) == 0) {
                    current_track.difficulty = "Hard";
                    current_track.instrument = current_section.substr(4);
                } else if (current_section.rfind("Expert", 0) == 0) {
                    current_track.difficulty = "Expert";
                    current_track.instrument = current_section.substr(6);
                } else {
                    current_track.difficulty = "Standard";
                    current_track.instrument = current_section;
                }
            }
            continue;
        }

        if (current_section == "Song") {
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                std::string key = line.substr(0, eq);
                std::string val = line.substr(eq + 1);
                while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
                while (!val.empty() && (val.front() == ' ' || val.front() == '\t' || val.front() == '"')) val.erase(0, 1);
                while (!val.empty() && (val.back() == ' ' || val.back() == '\t' || val.back() == '"')) val.pop_back();

                std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                if (item.title.empty() && key == "name") item.title = val;
                if (item.artist.empty() && key == "artist") item.artist = val;
                if (item.charter.empty() && key == "charter") item.charter = val;
                if (item.album.empty() && key == "album") item.album = val;
                if (item.year.empty() && key == "year") item.year = val;
                if (key == "offset") {
                    item.duration_seconds = std::strtod(val.c_str(), nullptr);
                }
            }
        } else if (current_section == "SyncTrack") {
            // Find start BPM
            if (item.bpm == 0.0 && line.find("B ") != std::string::npos) {
                size_t pos = line.find("B ");
                std::string bpm_str = line.substr(pos + 2);
                uint32_t raw = static_cast<uint32_t>(std::strtoul(bpm_str.c_str(), nullptr, 10));
                item.bpm = raw / 1000.0;
            }
        } else if (in_track) {
            if (line.find(" N ") != std::string::npos) {
                note_count++;
            }
        }
    }

    if (in_track && !current_track.track_name.empty()) {
        current_track.note_count = note_count;
        item.tracks.push_back(current_track);
    }
}

bool SongManager::scanSongs(const std::string& base_path) {
    m_songs.clear();

    std::vector<std::string> search_roots = {
        base_path,
        "songs",
        "song",
        "ms0:/PSP/GAME/cloneHero/" + base_path,
        "ms0:/PSP/GAME/cloneHero/songs",
        "ms0:/PSP/GAME/cloneHero/song",
        "./" + base_path,
        "./songs",
        "./song"
    };

    std::string root_dir;
    for (const auto& r : search_roots) {
        DIR* dir = opendir(r.c_str());
        if (dir) {
            closedir(dir);
            root_dir = r;
            break;
        }
    }

    if (root_dir.empty()) {
        root_dir = base_path;
    }

    DIR* dir = opendir(root_dir.c_str());
    if (!dir) return false;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        std::string folder_name = entry->d_name;
        std::string sub_path = root_dir + "/" + folder_name;

        // Check if directory
        struct stat st;
        if (stat(sub_path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
            continue;
        }

        // Look for notes.chart or chart.chart
        std::string chart_file = findFirstExisting({
            sub_path + "/notes.chart",
            sub_path + "/chart.chart",
            sub_path + "/notes.mid"
        });

        if (chart_file.empty()) {
            continue;
        }

        SongItem song;
        song.id = folder_name;
        song.folder_path = sub_path;
        song.chart_path = chart_file;
        song.bpm = 120.0;
        song.preview_start = 0.0;
        song.duration_seconds = 0.0;

        // Song.ini
        song.ini_path = findFirstExisting({
            sub_path + "/song.ini",
            sub_path + "/Song.ini"
        });
        if (!song.ini_path.empty()) {
            parseIniFile(song.ini_path, song);
        }

        // Audio stream: prioritize full song mix if available, then guitar stem
        song.audio_path = findFirstExisting({
            sub_path + "/song.opus",
            sub_path + "/guitar.opus",
            sub_path + "/music.opus",
            sub_path + "/audio.opus",
            sub_path + "/tracks.opus",
            sub_path + "/song.ogg",
            sub_path + "/guitar.ogg",
            sub_path + "/music.ogg",
            sub_path + "/audio.ogg",
            sub_path + "/tracks.ogg",
            sub_path + "/song.mp3",
            sub_path + "/guitar.mp3",
            sub_path + "/music.mp3",
            sub_path + "/audio.mp3"
        });

        // Album cover
        song.album_path = findFirstExisting({
            sub_path + "/album.png",
            sub_path + "/album.jpg",
            sub_path + "/cover.png",
            sub_path + "/cover.jpg"
        });

        // Inspect chart for tracks and title fallback
        inspectChartFile(song.chart_path, song);

        if (song.title.empty()) song.title = folder_name;
        if (song.artist.empty()) song.artist = "Unknown Artist";

        // Restore any saved high scores
        for (const auto& t : song.tracks) {
            std::string key = song.id + ":" + t.difficulty;
            auto it = m_scores.find(key);
            if (it != m_scores.end()) {
                song.best_scores[t.difficulty] = it->second;
            }
        }

        m_songs.push_back(song);
    }

    closedir(dir);

    // Sort songs alphabetically by title
    std::sort(m_songs.begin(), m_songs.end(), [](const SongItem& a, const SongItem& b) {
        return a.title < b.title;
    });

    return !m_songs.empty();
}

uint32_t SongManager::getBestScore(const std::string& song_id, const std::string& diff) const {
    std::string key = song_id + ":" + diff;
    auto it = m_scores.find(key);
    return (it != m_scores.end()) ? it->second : 0;
}

void SongManager::setBestScore(const std::string& song_id, const std::string& diff, uint32_t score) {
    std::string key = song_id + ":" + diff;
    if (score > m_scores[key]) {
        m_scores[key] = score;
        saveScores();
    }
}

void SongManager::saveScores(const std::string& filepath) {
    FILE* f = std::fopen(filepath.c_str(), "w");
    if (!f) return;

    for (const auto& pair : m_scores) {
        std::fprintf(f, "%s=%lu\n", pair.first.c_str(), static_cast<unsigned long>(pair.second));
    }
    std::fclose(f);
}

void SongManager::loadScores(const std::string& filepath) {
    FILE* f = std::fopen(filepath.c_str(), "r");
    if (!f) return;

    char line[256];
    while (std::fgets(line, sizeof(line), f)) {
        char* eq = std::strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        std::string key = line;
        uint32_t val = static_cast<uint32_t>(std::strtoul(eq + 1, nullptr, 10));
        m_scores[key] = val;
    }
    std::fclose(f);
}
