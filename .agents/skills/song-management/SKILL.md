---
name: song-management
description: >-
  Scan, organize, and validate song directories in PSP Hero. Use when adding new songs, validating album art, editing song.ini metadata, or checking directory structures.
---

# Song Management & Library Runbook

This skill covers how to structure, add, and validate songs in the PSP Hero library.

## 1. Song Directory Structure
Each song must reside in its own folder under `songs/`:
```text
songs/<Song Folder Name>/
├── notes.chart         # Note chart file (or chart.chart)
├── song.ogg            # Audio track (.ogg, .opus, or .mp3; guitar.* also accepted)
├── album.png           # Album cover art (or album.jpg, cover.png, cover.jpg)
└── song.ini            # Metadata configuration
```

## 2. File Requirements
- **Chart**: `notes.chart` (or `chart.chart`). If only `notes.mid` exists, run `./tools/mid2chart.py notes.mid notes.chart` first.
- **Audio**: Supported filenames in order of preference:
  `song.opus`, `guitar.opus`, `song.ogg`, `guitar.ogg`, `song.mp3`, `guitar.mp3`.
- **Album Cover**: `album.png`, `album.jpg`, `cover.png`, or `cover.jpg`. PNG and JPEG are supported via `stb_image`. Recommended resolution: 256x256 or 512x512.
- **`song.ini` format**:
  ```ini
  [song]
  name = Song Title
  artist = Artist Name
  album = Album Name
  year = 2024
  charter = Charter Name
  genre = Rock
  preview_start_time = 30000 ; in milliseconds
  ```

---

## 3. Validating the Entire Song Library
Run this quick Python script from the workspace root to check all songs for complete assets:
```bash
python3 -c '
import os

songs_dir = "songs"
folders = sorted([f for f in os.listdir(songs_dir) if os.path.isdir(os.path.join(songs_dir, f))])
print(f"Total songs detected: {len(folders)}")

all_ok = True
for idx, f in enumerate(folders):
    p = os.path.join(songs_dir, f)
    has_chart = os.path.exists(os.path.join(p, "notes.chart"))
    audio_candidates = ["song.opus", "guitar.opus", "song.ogg", "guitar.ogg", "song.mp3", "guitar.mp3"]
    audio = next((a for a in audio_candidates if os.path.exists(os.path.join(p, a))), None)
    album = next((a for a in ["album.png", "album.jpg", "cover.png", "cover.jpg"] if os.path.exists(os.path.join(p, a))), None)
    
    status = "OK" if (has_chart and audio and album) else "FAIL"
    if status == "FAIL":
        all_ok = False
    print(f"[{idx+1:02d}] {f:<40} | Chart: {str(has_chart):<5} | Audio: {str(audio):<12} | Cover: {str(album):<10} -> {status}")

print("\nAll songs validated:", all_ok)
'
```

---

## 4. High Score Persistence
- High scores are saved to `scores.dat` in binary format (`id:difficulty` mapped to score).
- Restored automatically whenever `SongManager::scanSongs()` executes.
