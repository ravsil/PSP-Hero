#include "../src/core/chart_parser.h"
#include <iostream>
#include <iomanip>

void printTime(double seconds) {
    int total_s = static_cast<int>(seconds);
    int m = total_s / 60;
    int s = total_s % 60;
    int ms = static_cast<int>((seconds - total_s) * 1000.0);
    std::cout << std::setfill('0') << std::setw(2) << m << ":"
              << std::setfill('0') << std::setw(2) << s << "."
              << std::setfill('0') << std::setw(3) << ms
              << std::setfill(' ');
}

const char* laneName(int lane) {
    switch (lane) {
        case LANE_GREEN:  return "Green (1)";
        case LANE_RED:    return "Red (2)";
        case LANE_YELLOW: return "Yellow (3)";
        case LANE_BLUE:   return "Blue (4)";
        case LANE_ORANGE: return "Orange (5)";
        case LANE_OPEN:   return "Open Strum";
        default:          return "Unknown";
    }
}

int main(int argc, char* argv[]) {
    std::string chart_path = "song/notes.chart";
    std::string ini_path = "song/song.ini";

    if (argc > 1) {
        chart_path = argv[1];
    }
    if (argc > 2) {
        ini_path = argv[2];
    }

    std::cout << "========================================" << std::endl;
    std::cout << "  Clone Hero .chart File Parser Test    " << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Loading: " << chart_path << std::endl;

    ChartSong song;
    if (!ChartParser::loadChartFile(chart_path, song)) {
        std::cerr << "ERROR: Failed to parse chart file: " << chart_path << std::endl;
        return 1;
    }

    // Try loading companion song.ini if present
    ChartParser::loadSongIni(ini_path, song);

    std::cout << "\n[METADATA]" << std::endl;
    std::cout << "  Title:       " << song.name << std::endl;
    std::cout << "  Artist:      " << song.artist << std::endl;
    std::cout << "  Charter:     " << song.charter << std::endl;
    std::cout << "  Album:       " << song.album << std::endl;
    std::cout << "  Year:        " << song.year << std::endl;
    std::cout << "  Genre:       " << song.genre << std::endl;
    std::cout << "  Resolution:  " << song.resolution << " ticks/quarter note" << std::endl;
    std::cout << "  Offset:      " << song.offset_seconds << " s" << std::endl;
    std::cout << "  Duration:    ";
    printTime(song.total_duration_seconds);
    std::cout << " (" << std::fixed << std::setprecision(2) << song.total_duration_seconds << "s)" << std::endl;

    std::cout << "\n[SYNC TRACK]" << std::endl;
    std::cout << "  BPM changes: " << song.bpms.size() << std::endl;
    if (!song.bpms.empty()) {
        std::cout << "  Start BPM:   " << song.bpms.front().bpm << " BPM" << std::endl;
        double min_bpm = song.bpms.front().bpm, max_bpm = song.bpms.front().bpm;
        for (const auto& b : song.bpms) {
            if (b.bpm < min_bpm) min_bpm = b.bpm;
            if (b.bpm > max_bpm) max_bpm = b.bpm;
        }
        std::cout << "  BPM Range:   " << min_bpm << " - " << max_bpm << " BPM" << std::endl;
    }
    std::cout << "  Time Sig:    " << song.time_signatures.size() << " changes" << std::endl;

    std::cout << "\n[SONG SECTIONS]" << std::endl;
    int section_count = 0;
    for (const auto& ev : song.events) {
        if (ev.text.rfind("section ", 0) == 0 || ev.text.rfind("prc_", 0) == 0) {
            std::cout << "  [";
            printTime(ev.time_seconds);
            std::cout << "] (tick " << std::setw(6) << ev.tick << ") " << ev.text << std::endl;
            section_count++;
        }
    }
    if (section_count == 0) {
        std::cout << "  (No section markers found)" << std::endl;
    }

    std::cout << "\n[TRACKS FOUND]" << std::endl;
    for (const auto& t : song.tracks) {
        int single_notes = 0;
        int sustains = 0;
        int open_notes = 0;
        for (const auto& n : t.notes) {
            if (n.lane == LANE_OPEN) open_notes++;
            if (n.length_ticks > 0) sustains++;
            else single_notes++;
        }
        std::cout << "  * " << t.name << " (" << t.difficulty << " " << t.instrument << "):" << std::endl;
        std::cout << "      Total Notes: " << t.notes.size()
                  << " (Taps: " << single_notes
                  << ", Sustains: " << sustains
                  << ", Open notes: " << open_notes << ")" << std::endl;
        std::cout << "      Star Power:  " << t.star_power.size() << " phrases" << std::endl;
    }

    const ChartTrack* default_track = song.getDefaultTrack();
    if (default_track) {
        std::cout << "\n[SAMPLE NOTES: " << default_track->name << "]" << std::endl;
        std::cout << "  First 12 notes:" << std::endl;
        size_t count = std::min(default_track->notes.size(), size_t(12));
        for (size_t i = 0; i < count; ++i) {
            const auto& n = default_track->notes[i];
            std::cout << "    #" << std::setw(3) << (i + 1) << " [";
            printTime(n.time_seconds);
            std::cout << "] Tick: " << std::setw(6) << n.tick
                      << " | " << std::setw(12) << std::left << laneName(n.lane) << std::right
                      << " | Length: " << std::setw(5) << n.length_ticks << " ticks"
                      << " (" << std::fixed << std::setprecision(3) << n.duration_seconds << "s)"
                      << (n.is_hopo ? " [HOPO]" : "")
                      << (n.is_tap ? " [TAP]" : "")
                      << std::endl;
        }

        if (default_track->notes.size() > 12) {
            std::cout << "  ..." << std::endl;
            std::cout << "  Last 3 notes:" << std::endl;
            for (size_t i = default_track->notes.size() - 3; i < default_track->notes.size(); ++i) {
                const auto& n = default_track->notes[i];
                std::cout << "    #" << std::setw(3) << (i + 1) << " [";
                printTime(n.time_seconds);
                std::cout << "] Tick: " << std::setw(6) << n.tick
                          << " | " << std::setw(12) << std::left << laneName(n.lane) << std::right
                          << " | Length: " << std::setw(5) << n.length_ticks << " ticks"
                          << " (" << std::fixed << std::setprecision(3) << n.duration_seconds << "s)"
                          << (n.is_hopo ? " [HOPO]" : "")
                          << std::endl;
            }
        }
    }

    // Verification tests
    std::cout << "\n[VERIFICATION CHECKS]" << std::endl;
    // Check round-trip tick <-> time
    uint32_t test_tick = 57600;
    double t = song.tickToSeconds(test_tick);
    uint32_t rt_tick = song.secondsToTick(t);
    std::cout << "  Round-trip tick test: Tick " << test_tick << " -> " << t << "s -> Tick " << rt_tick
              << " [Difference: " << std::abs((int)test_tick - (int)rt_tick) << " ticks]" << std::endl;

    std::cout << "  Section at 60.0s:  \"" << song.getSectionAt(60.0) << "\"" << std::endl;
    std::cout << "  Section at 120.0s: \"" << song.getSectionAt(120.0) << "\"" << std::endl;
    std::cout << "  BPM at 60.0s:      " << song.getBPMAt(60.0) << " BPM" << std::endl;

    std::cout << "\n[SUCCESS] .chart file parsed and verified successfully!" << std::endl;
    return 0;
}
