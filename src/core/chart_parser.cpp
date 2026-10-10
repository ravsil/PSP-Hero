#include "chart_parser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <iostream>
#include <map>

ChartSong::ChartSong() {
    resolution = 480;
    offset_seconds = 0.0;
    total_duration_seconds = 0.0;
}

double ChartSong::tickToSeconds(uint32_t tick) const {
    if (bpms.empty()) {
        double bpm = 120.0;
        return offset_seconds + (static_cast<double>(tick) / static_cast<double>(resolution)) * (60.0 / bpm);
    }

    // Binary search to find the last BPM event with tick <= target tick
    size_t low = 0;
    size_t high = bpms.size() - 1;
    size_t best = 0;

    while (low <= high) {
        size_t mid = low + (high - low) / 2;
        if (bpms[mid].tick <= tick) {
            best = mid;
            low = mid + 1;
        } else {
            if (mid == 0) break;
            high = mid - 1;
        }
    }

    const ChartBPM& b = bpms[best];
    uint32_t delta_ticks = tick - b.tick;
    double seconds_per_tick = 60.0 / (b.bpm * static_cast<double>(resolution));
    return b.time_seconds + delta_ticks * seconds_per_tick;
}

uint32_t ChartSong::secondsToTick(double seconds) const {
    if (bpms.empty()) {
        double bpm = 120.0;
        double s = seconds - offset_seconds;
        if (s < 0.0) return 0;
        return static_cast<uint32_t>(std::round(s / (60.0 / (bpm * resolution))));
    }

    if (seconds <= offset_seconds) return 0;

    // Find the BPM segment containing this timestamp
    size_t low = 0;
    size_t high = bpms.size() - 1;
    size_t best = 0;

    while (low <= high) {
        size_t mid = low + (high - low) / 2;
        if (bpms[mid].time_seconds <= seconds) {
            best = mid;
            low = mid + 1;
        } else {
            if (mid == 0) break;
            high = mid - 1;
        }
    }

    const ChartBPM& b = bpms[best];
    double delta_sec = seconds - b.time_seconds;
    if (delta_sec < 0.0) delta_sec = 0.0;
    double ticks_per_sec = (b.bpm * static_cast<double>(resolution)) / 60.0;
    return b.tick + static_cast<uint32_t>(std::round(delta_sec * ticks_per_sec));
}

double ChartSong::getBPMAt(double seconds) const {
    if (bpms.empty()) return 120.0;
    if (seconds <= bpms.front().time_seconds) return bpms.front().bpm;

    for (size_t i = bpms.size(); i > 0; --i) {
        if (seconds >= bpms[i - 1].time_seconds) {
            return bpms[i - 1].bpm;
        }
    }
    return bpms.front().bpm;
}

std::string ChartSong::getSectionAt(double seconds) const {
    std::string current_section;
    for (const auto& ev : events) {
        if (ev.time_seconds <= seconds) {
            // Check if it's a section marker
            const std::string prefix = "section ";
            if (ev.text.rfind(prefix, 0) == 0) {
                current_section = ev.text.substr(prefix.length());
            } else if (ev.text.rfind("prc_", 0) == 0) {
                current_section = ev.text.substr(4);
            }
        } else {
            break;
        }
    }
    return current_section;
}

const ChartTrack* ChartSong::getTrack(const std::string& track_name) const {
    for (const auto& t : tracks) {
        if (t.name == track_name) {
            return &t;
        }
    }
    return nullptr;
}

const ChartTrack* ChartSong::getDefaultTrack() const {
    // Priority order for guitar tracks
    static const char* preferred[] = {
        "ExpertSingle", "HardSingle", "MediumSingle", "EasySingle",
        "ExpertDoubleGuitar", "HardDoubleGuitar", "ExpertBass", nullptr
    };

    for (int i = 0; preferred[i] != nullptr; ++i) {
        const ChartTrack* t = getTrack(preferred[i]);
        if (t != nullptr && !t->notes.empty()) return t;
    }

    // Fallback to first non-empty track
    for (const auto& t : tracks) {
        if (!t.notes.empty()) return &t;
    }

    if (!tracks.empty()) return &tracks.front();
    return nullptr;
}

void ChartSong::finalizeTiming() {
    // 1. Sort BPM events by tick
    std::sort(bpms.begin(), bpms.end(), [](const ChartBPM& a, const ChartBPM& b) {
        return a.tick < b.tick;
    });

    // Ensure there is at least one BPM marker at tick 0
    if (bpms.empty() || bpms.front().tick > 0) {
        ChartBPM default_bpm;
        default_bpm.tick = 0;
        default_bpm.bpm_raw = 120000;
        default_bpm.bpm = 120.0;
        default_bpm.time_seconds = offset_seconds;
        bpms.insert(bpms.begin(), default_bpm);
    }

    // 2. Precompute exact timestamps for all BPM changes
    double current_time = offset_seconds;
    uint32_t prev_tick = 0;
    double prev_bpm = bpms.front().bpm;

    for (size_t i = 0; i < bpms.size(); ++i) {
        uint32_t dt = bpms[i].tick - prev_tick;
        current_time += (static_cast<double>(dt) / static_cast<double>(resolution)) * (60.0 / prev_bpm);
        bpms[i].time_seconds = current_time;
        prev_tick = bpms[i].tick;
        prev_bpm = bpms[i].bpm;
    }

    // 3. Precompute events timing
    std::sort(events.begin(), events.end(), [](const ChartEvent& a, const ChartEvent& b) {
        return a.tick < b.tick;
    });
    for (auto& ev : events) {
        ev.time_seconds = tickToSeconds(ev.tick);
    }

    // 4. Precompute time signatures timing
    std::sort(time_signatures.begin(), time_signatures.end(), [](const ChartTimeSignature& a, const ChartTimeSignature& b) {
        return a.tick < b.tick;
    });
    for (auto& ts : time_signatures) {
        ts.time_seconds = tickToSeconds(ts.tick);
    }

    // 5. Precompute notes timing and HOPO detection for all tracks
    total_duration_seconds = 0.0;
    uint32_t hopo_threshold_ticks = (resolution * 4) / 12; // Standard 1/12th measure threshold

    for (auto& track : tracks) {
        std::sort(track.notes.begin(), track.notes.end(), [](const ChartNote& a, const ChartNote& b) {
            if (a.tick != b.tick) return a.tick < b.tick;
            return a.lane < b.lane;
        });

        int prev_tick_val = -1;
        int prev_lane = -1;
        bool prev_was_chord = false;

        for (size_t i = 0; i < track.notes.size(); ++i) {
            auto& n = track.notes[i];
            n.time_seconds = tickToSeconds(n.tick);
            if (n.length_ticks > 0) {
                double end_time = tickToSeconds(n.tick + n.length_ticks);
                n.duration_seconds = end_time - n.time_seconds;
            } else {
                n.duration_seconds = 0.0;
            }
            n.hit = false;
            n.missed = false;
            n.sustaining = false;
            n.sustain_dropped = false;
            n.sustain_scored_time = 0.0;

            // Check chord status
            bool is_chord = false;
            if (i + 1 < track.notes.size() && track.notes[i + 1].tick == n.tick) {
                is_chord = true;
            }
            if (i > 0 && track.notes[i - 1].tick == n.tick) {
                is_chord = true;
            }

            // Natural HOPO calculation:
            // Must be within threshold, not a chord, and fret is different from previous note
            if (prev_tick_val >= 0 && !is_chord && !prev_was_chord) {
                uint32_t delta = n.tick - prev_tick_val;
                if (delta <= hopo_threshold_ticks && n.lane != prev_lane) {
                    n.is_hopo = true;
                }
            }

            // Forced HOPO toggle (Lane 5)
            // (Note: if user had N 5 modifier at this tick, is_hopo was inverted in parser)

            if (n.lane <= 4 || n.lane == LANE_OPEN) {
                prev_tick_val = n.tick;
                prev_lane = n.lane;
                prev_was_chord = is_chord;
            }

            double note_end = n.time_seconds + n.duration_seconds;
            if (note_end > total_duration_seconds) {
                total_duration_seconds = note_end;
            }
        }

        // Star power timing
        for (auto& sp : track.star_power) {
            sp.time_seconds = tickToSeconds(sp.tick);
            sp.duration_seconds = tickToSeconds(sp.tick + sp.length_ticks) - sp.time_seconds;
        }
    }

    // Add brief margin after last note
    total_duration_seconds += 3.0;
}

std::string ChartParser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string ChartParser::stripQuotes(const std::string& str) {
    std::string s = trim(str);
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

std::string ChartParser::stripColorTags(const std::string& input) {
    std::string result;
    bool in_tag = false;
    for (char c : input) {
        if (c == '<') {
            in_tag = true;
        } else if (c == '>') {
            in_tag = false;
        } else if (!in_tag) {
            result += c;
        }
    }
    return result;
}

bool ChartParser::loadChartFile(const std::string& filepath, ChartSong& out_song) {
    std::ifstream file(filepath.c_str(), std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "ChartParser: Failed to open file: " << filepath << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return parseChartString(buffer.str(), out_song);
}

bool ChartParser::parseChartString(const std::string& content, ChartSong& out_song) {
    out_song = ChartSong();

    std::istringstream stream(content);
    std::string line;
    std::string current_section;

    // Storage for temporary modifier tags per track
    // Map tick -> (has_forced_hopo, has_tap)
    struct ModifierMap {
        std::vector<uint32_t> forced_ticks;
        std::vector<uint32_t> tap_ticks;
    };
    std::map<std::string, ModifierMap> track_modifiers;

    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        // Skip UTF-8 BOM if present on first line
        if (line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) {
            line = trim(line.substr(3));
            if (line.empty()) continue;
        }

        // Section header e.g. [Song], [SyncTrack], [ExpertSingle]
        if (line.front() == '[' && line.back() == ']') {
            current_section = line.substr(1, line.size() - 2);
            continue;
        }

        // Ignore block braces
        if (line == "{" || line == "}") continue;

        // Key = Value syntax
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq_pos));
        std::string value = trim(line.substr(eq_pos + 1));

        if (current_section == "Song") {
            if (key == "Name") {
                out_song.name = stripQuotes(value);
            } else if (key == "Artist") {
                out_song.artist = stripQuotes(value);
            } else if (key == "Charter") {
                out_song.charter = stripColorTags(stripQuotes(value));
            } else if (key == "Album") {
                out_song.album = stripQuotes(value);
            } else if (key == "Year") {
                std::string y = stripQuotes(value);
                // Strip leading comma/space if present (e.g. ", 2001")
                size_t y_idx = y.find_first_not_of(" ,");
                if (y_idx != std::string::npos) y = y.substr(y_idx);
                out_song.year = y;
            } else if (key == "Offset") {
                out_song.offset_seconds = std::atof(value.c_str());
            } else if (key == "Resolution") {
                out_song.resolution = static_cast<uint32_t>(std::atol(value.c_str()));
                if (out_song.resolution == 0) out_song.resolution = 480;
            } else if (key == "Difficulty") {
                out_song.difficulty = static_cast<uint32_t>(std::atol(value.c_str()));
            } else if (key == "PreviewStart") {
                out_song.preview_start = std::atof(value.c_str());
            } else if (key == "PreviewEnd") {
                out_song.preview_end = std::atof(value.c_str());
            } else if (key == "Genre") {
                out_song.genre = stripQuotes(value);
            } else if (key == "MediaType") {
                out_song.media_type = stripQuotes(value);
            } else if (key == "GuitarStream") {
                out_song.guitar_stream = stripQuotes(value);
            }
        } else if (current_section == "SyncTrack") {
            uint32_t tick = static_cast<uint32_t>(std::strtoul(key.c_str(), nullptr, 10));
            std::istringstream val_stream(value);
            std::string code;
            val_stream >> code;

            if (code == "B") {
                uint32_t bpm_raw = 0;
                val_stream >> bpm_raw;
                ChartBPM b;
                b.tick = tick;
                b.bpm_raw = bpm_raw;
                b.bpm = static_cast<double>(bpm_raw) / 1000.0;
                b.time_seconds = 0.0;
                out_song.bpms.push_back(b);
            } else if (code == "TS") {
                uint32_t num = 4;
                uint32_t den_exp = 2; // Default 2^2 = 4
                val_stream >> num;
                if (!(val_stream >> den_exp)) den_exp = 2;

                ChartTimeSignature ts;
                ts.tick = tick;
                ts.numerator = num;
                ts.denominator = 1u << den_exp;
                ts.time_seconds = 0.0;
                out_song.time_signatures.push_back(ts);
            }
        } else if (current_section == "Events") {
            uint32_t tick = static_cast<uint32_t>(std::strtoul(key.c_str(), nullptr, 10));
            std::istringstream val_stream(value);
            std::string code;
            val_stream >> code;

            if (code == "E") {
                std::string event_text;
                std::getline(val_stream, event_text);
                event_text = stripQuotes(event_text);
                ChartEvent ev;
                ev.tick = tick;
                ev.time_seconds = 0.0;
                ev.text = event_text;
                out_song.events.push_back(ev);
            }
        } else if (!current_section.empty()) {
            // Instrument / Difficulty Track
            uint32_t tick = static_cast<uint32_t>(std::strtoul(key.c_str(), nullptr, 10));
            std::istringstream val_stream(value);
            std::string code;
            val_stream >> code;

            // Find or create track
            ChartTrack* track = nullptr;
            for (auto& t : out_song.tracks) {
                if (t.name == current_section) {
                    track = &t;
                    break;
                }
            }
            if (!track) {
                ChartTrack new_track;
                new_track.name = current_section;
                // Parse difficulty & instrument from section name
                if (current_section.find("Expert") != std::string::npos) new_track.difficulty = "Expert";
                else if (current_section.find("Hard") != std::string::npos) new_track.difficulty = "Hard";
                else if (current_section.find("Medium") != std::string::npos) new_track.difficulty = "Medium";
                else if (current_section.find("Easy") != std::string::npos) new_track.difficulty = "Easy";

                if (current_section.find("Single") != std::string::npos) new_track.instrument = "Lead Guitar";
                else if (current_section.find("DoubleGuitar") != std::string::npos) new_track.instrument = "Co-op Guitar";
                else if (current_section.find("DoubleBass") != std::string::npos) new_track.instrument = "Bass";
                else if (current_section.find("Drums") != std::string::npos) new_track.instrument = "Drums";

                out_song.tracks.push_back(new_track);
                track = &out_song.tracks.back();
            }

            if (code == "N") {
                int lane = 0;
                uint32_t length = 0;
                val_stream >> lane >> length;

                if (lane == LANE_FORCED) {
                    track_modifiers[current_section].forced_ticks.push_back(tick);
                } else if (lane == LANE_TAP) {
                    track_modifiers[current_section].tap_ticks.push_back(tick);
                } else {
                    ChartNote note;
                    note.tick = tick;
                    note.lane = lane;
                    note.length_ticks = length;
                    note.time_seconds = 0.0;
                    note.duration_seconds = 0.0;
                    note.is_hopo = false;
                    note.is_tap = false;
                    note.hit = false;
                    note.missed = false;
                    note.sustaining = false;
                    note.sustain_dropped = false;
                    note.sustain_scored_time = 0.0;
                    track->notes.push_back(note);
                }
            } else if (code == "S") {
                int type = 0;
                uint32_t length = 0;
                val_stream >> type >> length;
                if (type == 2) {
                    ChartStarPower sp;
                    sp.tick = tick;
                    sp.length_ticks = length;
                    sp.time_seconds = 0.0;
                    sp.duration_seconds = 0.0;
                    track->star_power.push_back(sp);
                }
            }
        }
    }

    // Apply modifiers (forced HOPOs and Taps) to parsed notes
    for (auto& track : out_song.tracks) {
        auto it = track_modifiers.find(track.name);
        if (it != track_modifiers.end()) {
            const auto& mod = it->second;
            for (auto& note : track.notes) {
                // Check forced HOPO
                if (std::find(mod.forced_ticks.begin(), mod.forced_ticks.end(), note.tick) != mod.forced_ticks.end()) {
                    note.is_hopo = !note.is_hopo; // Invert HOPO status
                }
                // Check Tap note
                if (std::find(mod.tap_ticks.begin(), mod.tap_ticks.end(), note.tick) != mod.tap_ticks.end()) {
                    note.is_tap = true;
                }
            }
        }
    }

    out_song.finalizeTiming();
    return true;
}

bool ChartParser::loadSongIni(const std::string& filepath, ChartSong& song) {
    std::ifstream file(filepath.c_str());
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') continue;

        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq_pos));
        std::string val = trim(line.substr(eq_pos + 1));
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        if (key == "name" && song.name.empty()) {
            song.name = stripQuotes(val);
        } else if (key == "artist" && song.artist.empty()) {
            song.artist = stripQuotes(val);
        } else if (key == "charter") {
            song.charter = stripColorTags(stripQuotes(val));
        } else if (key == "album" && song.album.empty()) {
            song.album = stripQuotes(val);
        } else if (key == "year" && song.year.empty()) {
            song.year = stripQuotes(val);
        } else if (key == "genre" && song.genre.empty()) {
            song.genre = stripQuotes(val);
        } else if (key == "song_length") {
            double ms = std::atof(val.c_str());
            if (ms > 0 && song.total_duration_seconds < (ms / 1000.0)) {
                song.total_duration_seconds = ms / 1000.0;
            }
        }
    }
    return true;
}
