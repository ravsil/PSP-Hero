---
name: chart-tools
description: >-
  Convert MIDI charts (.mid) to .chart, validate song charts, inspect tempo maps, sections, difficulties, and verify gameplay tracks. Use when adding songs, converting charts, or debugging chart parsing.
---

# Chart Tools & Conversion Runbook

This skill covers working with `.chart` and `.mid` files in PSP Hero, converting MIDI tracks to `.chart` format, and testing parser accuracy.

## 1. Converting MIDI Charts (`notes.mid` to `notes.chart`)
Guitar Hero and Rock Band charts frequently use `notes.mid`. PSP Hero includes a dedicated MIDI-to-chart converter in `tools/mid2chart.py`.

### Command:
```bash
./tools/mid2chart.py <path/to/notes.mid> <path/to/notes.chart> "[Song Title]"
```

### Example:
```bash
./tools/mid2chart.py "songs/Dragonforce - Through The Fire & Flames/notes.mid" \
                     "songs/Dragonforce - Through The Fire & Flames/notes.chart" \
                     "Through the Fire and Flames"
```

### MIDI Track Mapping Details:
- The converter automatically extracts `PART GUITAR` (or `T1 GEMS` / `GUITAR`).
- Fret pitches in MIDI:
  - **Easy**: 60 (Green), 61 (Red), 62 (Yellow), 63 (Blue), 64 (Orange)
  - **Medium**: 72 (Green), 73 (Red), 74 (Yellow), 75 (Blue), 76 (Orange)
  - **Hard**: 84 (Green), 85 (Red), 86 (Yellow), 87 (Blue), 88 (Orange)
  - **Expert**: 96 (Green), 97 (Red), 98 (Yellow), 99 (Blue), 100 (Orange)
- **Star Power / Overdrive**: MIDI note 116.
- **Tempo Map**: Extracted from MIDI track 0 (microsecond-per-quarter tempo events converted to millihertz BPM).
- **Sections**: Extracted from track text/lyric meta-events (`[section ...]`, `[prc_...]`).

---

## 2. Testing and Verifying `.chart` Files
To verify that a chart parses cleanly without errors and check note counts and timings:

### Command:
```bash
./test_parser "songs/<Song Name>/notes.chart"
```

### Output Verified by `test_parser`:
- Metadata: Title, Artist, Charter, Album, Year, Resolution (ticks/beat).
- Sync Track: Total BPM changes, starting BPM, BPM range, Time Signatures.
- Song Sections: Timestamps and section names.
- Tracks Found: Lists each difficulty (`EasySingle`, `MediumSingle`, `HardSingle`, `ExpertSingle`) with note counts, sustain counts, tap markers, and Star Power phrases.
- Tick-to-time round-trip integrity test (must report 0 tick difference).
