#ifndef CHART_PARSER_H
#define CHART_PARSER_H

#include <string>
#include <vector>
#include <cstdint>

// Note fret / lane definitions for 5-fret guitar charts
enum ChartLane {
    LANE_GREEN   = 0,
    LANE_RED     = 1,
    LANE_YELLOW  = 2,
    LANE_BLUE    = 3,
    LANE_ORANGE  = 4,
    LANE_FORCED  = 5, // HOPO toggle flag
    LANE_TAP     = 6, // Tap note flag
    LANE_OPEN    = 7, // Open strum note (purple bar across highway)
    LANE_COUNT   = 8
};

// Represents a single note or modifier in a track
struct ChartNote {
    uint32_t tick;
    uint32_t length_ticks;
    int lane;                 // 0..4 (frets), 7 (open note)
    double time_seconds;      // Exact timestamp in song (seconds)
    double duration_seconds;  // Exact sustain length (seconds)
    bool is_hopo;             // Hammer-on / Pull-off
    bool is_tap;              // Tap note
    bool hit;                 // Hit state during gameplay / POC
    bool missed;              // Missed state during gameplay / POC
    bool sustaining;          // Actively being held during sustain
    bool sustain_dropped;     // Fret released early or missed
    double sustain_scored_time; // Last timestamp up to which sustain points were scored
};

// Represents a Star Power phrase
struct ChartStarPower {
    uint32_t tick;
    uint32_t length_ticks;
    double time_seconds;
    double duration_seconds;
};

// Represents a BPM tempo marker from [SyncTrack]
struct ChartBPM {
    uint32_t tick;
    uint32_t bpm_raw;         // e.g. 120210 represents 120.210 BPM
    double bpm;               // 120.210
    double time_seconds;      // Exact timestamp when this BPM change takes effect
};

// Represents a Time Signature marker from [SyncTrack]
struct ChartTimeSignature {
    uint32_t tick;
    uint32_t numerator;
    uint32_t denominator;
    double time_seconds;
};

// Represents a global or track event (e.g. section markers, lyrics)
struct ChartEvent {
    uint32_t tick;
    double time_seconds;
    std::string text;
};

// Represents an instrument / difficulty track (e.g. [ExpertSingle])
struct ChartTrack {
    std::string name;                     // e.g. "ExpertSingle", "HardSingle"
    std::string instrument;               // e.g. "Single", "DoubleGuitar", "Drums"
    std::string difficulty;               // e.g. "Expert", "Hard", "Medium", "Easy"
    std::vector<ChartNote> notes;
    std::vector<ChartStarPower> star_power;
};

// Root representation of a parsed .chart song
class ChartSong {
public:
    // Metadata from [Song]
    std::string name;
    std::string artist;
    std::string charter;
    std::string album;
    std::string year;
    std::string genre;
    std::string media_type;
    std::string guitar_stream;
    double offset_seconds = 0.0;
    uint32_t resolution = 480;            // Ticks per quarter note beat
    uint32_t difficulty = 0;
    double preview_start = 0.0;
    double preview_end = 0.0;

    // Timing synchronization
    std::vector<ChartBPM> bpms;
    std::vector<ChartTimeSignature> time_signatures;
    std::vector<ChartEvent> events;

    // Instrument / difficulty tracks
    std::vector<ChartTrack> tracks;

    // Total song duration derived from chart notes and BPM
    double total_duration_seconds = 0.0;

    ChartSong();

    // Timing conversion utilities
    double tickToSeconds(uint32_t tick) const;
    uint32_t secondsToTick(double seconds) const;

    // Get current BPM at a given song time (seconds)
    double getBPMAt(double seconds) const;

    // Get section event name active at a given song time (or empty if none)
    std::string getSectionAt(double seconds) const;

    // Find track by name (e.g. "ExpertSingle") or get the first available track
    const ChartTrack* getTrack(const std::string& track_name) const;
    const ChartTrack* getDefaultTrack() const;

    // Helper to calculate precomputed times for all elements
    void finalizeTiming();
};

// Chart and song.ini parser
class ChartParser {
public:
    // Loads and parses a .chart file. Returns true on success.
    static bool loadChartFile(const std::string& filepath, ChartSong& out_song);

    // Parses .chart contents from a memory buffer / string.
    static bool parseChartString(const std::string& content, ChartSong& out_song);

    // Optional companion song.ini parser to augment metadata.
    static bool loadSongIni(const std::string& filepath, ChartSong& song);

    // Strips HTML/formatting color tags from strings like "<color=#ff0000>Name</color>"
    static std::string stripColorTags(const std::string& input);

private:
    static std::string trim(const std::string& str);
    static std::string stripQuotes(const std::string& str);
};

#endif // CHART_PARSER_H

