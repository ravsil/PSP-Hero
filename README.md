# Clone Hero PSP (Proof of Concept)

A lightweight, hardware-accelerated rhythm game engine and `.chart` parser for the PlayStation Portable (PSP), compatible with real PSP hardware and the PPSSPP emulator.

---

## 1. How the `.chart` File Format Works

The `.chart` file format is a plaintext specification originally introduced for *Guitar Hero* customs and standardized by tools like **Moonscraper** and games like **Clone Hero**.

### File Structure & Sections

1. **`[Song]`** — Metadata & Core Timing Settings:
   - `Resolution`: Ticks per quarter note beat (standard is `480`).
   - `Offset`: Time in seconds to delay the chart relative to the audio file.
   - Metadata tags: `Name`, `Artist`, `Charter`, `Album`, `Year`, `Genre`, `GuitarStream`.
2. **`[SyncTrack]`** — Tempo and Time Signatures:
   - `tick = B <bpm_times_1000>`: Tempo changes. For instance, `0 = B 120210` means 120.210 BPM at tick 0.
   - `tick = TS <numerator> [<den_exp>]`: Time signature changes (default `4/4`).
3. **`[Events]`** — Global Game Events:
   - `tick = E "section <name>"`: Section markers (e.g. `section Intro`, `section Chorus 1`).
4. **Instrument & Difficulty Tracks** (e.g. `[ExpertSingle]`, `[HardSingle]`):
   - `tick = N <lane> <length_ticks>`: Note placements and modifiers.
   - `tick = S 2 <length_ticks>`: Star Power phrases.

### Fret / Lane Mapping

| Code | Type / Lane | Description |
|---|---|---|
| `N 0` | Green | Fret 1 (D-Pad Left / L Trigger) |
| `N 1` | Red | Fret 2 (D-Pad Down) |
| `N 2` | Yellow | Fret 3 (Triangle) |
| `N 3` | Blue | Fret 4 (Circle) |
| `N 4` | Orange | Fret 5 (Cross / R Trigger) |
| `N 5` | Forced HOPO | Inverts hammer-on / pull-off state for notes at this tick |
| `N 6` | Tap Note | Marks notes at this tick as tappable without strumming |
| `N 7` | Open Strum | Open note (purple horizontal bar across all lanes) |
| `S 2` | Star Power | Star power phrase covering `<length>` ticks |

### Exact Timing Calculation

Because tempo can change throughout the song, timestamps are precomputed by integrating over BPM segments:

$$\Delta t = \frac{\Delta \text{ticks}}{\text{Resolution}} \times \frac{60}{\text{BPM}}$$

The parser precomputes exact seconds for all 97 tempo changes and 1,160 notes on load, allowing $O(\log N)$ binary search queries and zero runtime performance overhead.

---

## 2. Compilation Environment (Docker)

No local PSP toolchain installation is required. The project uses the official Docker image `pspdev/pspdev:latest`, which has been pulled and verified.

### Build the PSP EBOOT.PBP
To compile using Docker:

```bash
./docker-build.sh
```

Or directly via `docker run`:

```bash
docker run --rm --user "$(id -u):$(id -g)" -v "$(pwd):/build" -w /build pspdev/pspdev:latest make
```

This compiles `cloneHero.elf` and generates **`EBOOT.PBP`** using the PSP SDK tools (`mksfoex`, `psp-strip`, `pack-pbp`).

### Build the Native Desktop Parser Test Tool
You can also build and run the parser natively on Linux with `g++` to inspect chart statistics and verify chart integrity:

```bash
make -f Makefile.native run
```

---

## 3. Running on PSP / PPSSPP

1. Copy the game folder containing `EBOOT.PBP` and `song/` to your Memory Stick:
   ```
   ms0:/PSP/GAME/cloneHero/
   ├── EBOOT.PBP
   └── song/
       ├── notes.chart
       ├── song.ini
       ├── guitar.opus
       └── album.jpg
   ```
2. Launch **Clone Hero PSP** from the PSP XMB (under Game > Memory Stick) or open the folder in the **PPSSPP** emulator.

---

## 4. Controls

| Button | Action |
|---|---|
| **START** | Pause / Resume playback |
| **SELECT** | Toggle **Autoplay** mode (watch chart demo vs manual play) |
| **L / R TRIGGERS** | Scrub / Seek -5 seconds / +5 seconds |
| **SQUARE** *(when paused)* | Restart song from the beginning |
| **D-Pad Left** | Green Fret |
| **D-Pad Down / Up** | Red Fret / Strum |
| **Triangle** | Yellow Fret |
| **Circle** | Blue Fret |
| **Cross** | Orange Fret |
| **Square** | Open Strum Note |

---

## 5. Architecture

- [`src/chart_parser.h`](file:///home/rafael/cloneHero/src/chart_parser.h) / [`src/chart_parser.cpp`](file:///home/rafael/cloneHero/src/chart_parser.cpp): Plaintext `.chart` and `song.ini` parser, UTF-8 BOM handling, BPM integration, HOPO / tap identification.
- [`src/highway_renderer.h`](file:///home/rafael/cloneHero/src/highway_renderer.h) / [`src/highway_renderer.cpp`](file:///home/rafael/cloneHero/src/highway_renderer.cpp): Hardware-accelerated PSP GU renderer (perspective highway, notes, open bars, sustains, hit sparks, HUD, and 8x8 font rendering).
- [`src/audio_player.h`](file:///home/rafael/cloneHero/src/audio_player.h) / [`src/audio_player.cpp`](file:///home/rafael/cloneHero/src/audio_player.cpp): High-performance Opus streaming engine:
  - Boosts PSP CPU clock to **333MHz** (`scePowerSetClockFrequency(333, 333, 166)`).
  - Stereo linear resampler (48kHz Opus source to 44.1kHz PSP native rate).
  - Large 32,768 frame (~740ms) stereo ring buffer preventing buffer underruns.
  - Native hardware audio output (`sceAudioChReserve` + `sceAudioOutputPannedBlocking`).
- [`src/font8x8.h`](file:///home/rafael/cloneHero/src/font8x8.h): Self-contained public-domain 8x8 bitmap font.
- [`src/main.cpp`](file:///home/rafael/cloneHero/src/main.cpp): Application loop, input processing, hit window detection, and scoring system.

