# PSP Hero

A full-featured, hardware-accelerated rhythm game engine for the Sony PlayStation Portable (PSP) and PPSSPP emulator, inspired by *Clone Hero* and *Guitar Hero*.

Built from the ground up in C++17 for the MIPS R4000 processor (333 MHz), **PSP Hero** delivers smooth 60 FPS 3D highway perspective rendering, low-latency audio streaming, dynamic chart parsing, and custom button mapping.

> [!WARNING]
> **Notice**: This project was developed mostly using AI.

---

## Features

- **Hardware-Accelerated 3D Highway**:
  - True perspective projection powered by native PSP GU (`pspgu` / `pspgum`).
  - Dynamic note gems (standard strum notes, Hammer-Ons/Pull-Offs, Tap notes, and purple open strum bars).
  - Multi-lane sustain tails with real-time hold scoring and particle sparks.
  - Reactive lane glow lighting activated dynamically when frets are pressed/fired.
  - Continuous rotating 5-segment glowing background wheel.

- **Audio Engine & Multi-Format Decoding**:
  - **Opus** (`.opus`, 48,000 Hz) via `libopusfile` / `libopus` (dithering disabled for optimal MIPS performance).
  - **Ogg Vorbis** (`.ogg`, 44,100 Hz / 48,000 Hz) via robust embedded `stb_vorbis`.
  - **MPEG Audio Layer 3** (`.mp3`) via header-only `minimp3_ex.h`.
  - Native hardware Sample Rate Converter channel (`sceAudioSRCChReserve`) for stutter-free stereo playback.
  - Dedicated hardware channel for sound effects (menu navigation, star power cues, streak loss pick slide).
  - Microsecond audio-visual synchronization using `psprtc` hardware tick counter.

- **Complete `.chart` Engine**:
  - Full Moonscraper / Clone Hero `.chart` file support.
  - Dynamic tempo maps (`[SyncTrack]` BPM changes pre-integrated into exact seconds).
  - Time signature changes, section markers, and Star Power phrases (`S 2`).
  - Note modifiers: HOPO forced markers (`N 5`), tap notes (`N 6`), open notes (`N 7`).
  - Python conversion script included (`tools/mid2chart.py`) to convert standard MIDI charts (`.mid`) to `.chart`.

- **Rich Game UI & Menus**:
  - **Main Menu**: Quick Play (picks a random song), Song Select, and Options.
  - **Song Browser**: Instant navigation, wrap-around scrolling, L/R trigger page skipping, and cached album artwork.
  - **Dynamic Difficulty Selection**: Automatically detects available difficulties (Easy, Medium, Hard, Expert) for each song.
  - **In-Game HUD**: Circular Multiplier Orb (1x–4x), Rock Meter with responsive needle, Star Power gauge, real-time accuracy percentage, and score counters.
  - **Pause Menu & 3-2-1 Countdown**: Resume with an animated 3-2-1 countdown, restart, or quit back to song selection.
  - **Star Power Mode**: Electric cyan highway rails, cyan note gems, and 2x score multiplier.

- **Fully Customizable Controls**:
  - Dedicated **Button Remapper** menu supporting any PSP button assignment.
  - Circle ($\bigcirc$) is treated as a standard assignable button.
  - Toggle between **Strum Mode** (fret + strum required) and **Tap Mode** (press frets directly to hit notes).

---

## Directory Structure

```text
cloneHero/
├── .agents/               # Agent guidelines, rules, and runbooks
│   ├── AGENTS.md          # Technical guidelines and architecture invariants
│   └── skills/            # Procedural runbooks (build, audio, charts, deploy)
├── src/
│   ├── core/              # Audio playback, chart parser, song manager, config, SFX
│   │   ├── audio_player.cpp / .h    # Multi-format streaming audio player
│   │   ├── chart_parser.cpp / .h    # .chart specification parser
│   │   ├── config.cpp / .h          # Button remapping and options persistence
│   │   ├── minimp3.h / minimp3_ex.h # Public-domain MP3 decoder
│   │   ├── song_manager.cpp / .h    # Directory scanner and album art cache
│   │   ├── sound_effects.cpp / .h   # PCM sound effect playback
│   │   ├── sound_effects_data.h     # Embedded synthesized PCM audio tables
│   │   └── stb_vorbis.c / .h        # Embedded Ogg Vorbis decoder
│   ├── game/              # Highway perspective rendering, notes, sustains, HUD
│   │   ├── gameplay.cpp / .h        # Scoring, multiplier, rock meter, sessions
│   │   └── highway_renderer.cpp / .h# 3D projection, gems, receptors, particles
│   ├── render/            # Graphics and font rendering
│   │   ├── font8x8.h                # Public-domain 8x8 bitmap font
│   │   ├── renderer.cpp / .h        # Hardware Gu/Gum rendering wrapper
│   │   └── stb_image.h              # Image decoder for album art
│   ├── ui/                # Prototype-styled menus, difficulty selector, HUD
│   │   ├── menu_screens.cpp / .h    # Title, menu, song browser, options, remapper
│   │   └── ui_common.h              # Color constants and UI layout macros
│   └── main.cpp           # Game initialization, main loop, state machine
├── songs/                 # Song library directory (git-ignored for user songs)
├── tools/                 # Development and conversion utilities
│   ├── mid2chart.py       # Python MIDI-to-.chart converter
│   └── test_parser.cpp    # Standalone host test utility for chart verification
├── ICON0.PNG              # 144x80 official XMB game icon
├── Makefile               # PSPSDK Makefile for PRX and encrypted EBOOT packaging
├── Makefile.native        # Host testing Makefile for desktop parser debugging
├── docker-build.sh        # Synchronous Docker compilation script (pspdev/pspdev:latest)
├── LICENSE                # MIT License
└── README.md              # Project documentation
```

---

## Controls

### Default Layout (Handheld / Guitarist Grip)

| PSP Button | In-Game Action | Menu Action |
|---|---|---|
| **D-Pad Left** | Green Fret | Navigate Left / Page Up |
| **D-Pad Down** | Red Fret | Navigate Down |
| **Triangle ($\triangle$)** | Yellow Fret | Back / Exit Menu |
| **Circle ($\bigcirc$)** | Blue Fret | Select / Confirm |
| **Cross ($\times$)** | Orange Fret | Select / Confirm |
| **D-Pad Up** | Strum Up | Navigate Up |
| **D-Pad Right** | Strum Down | Navigate Right / Page Down |
| **L Trigger** | Star Power | Page Up in Song List |
| **R Trigger** | Open Strum / Star Power | Page Down in Song List |
| **START** | Pause | Start Game / Confirm |
| **SELECT** | Toggle Autoplay Mode | — |

*All gameplay actions can be freely rebound in **Options > Button Remapper**.*

---

## Adding Songs

PSP Hero reads songs from `songs/` (with fallback to `song/`). Each song must be stored in its own subfolder:

```text
ms0:/PSP/GAME/cloneHero/songs/
└── Through the Fire and Flames/
    ├── notes.chart         # Note chart (or chart.chart)
    ├── song.opus           # Audio file (.opus, .ogg, or .mp3)
    ├── song.ini            # Metadata: name, artist, album, charter (optional)
    └── album.png           # Album cover art, PNG/JPG up to 512x512 (optional)
```

### Audio Support
The engine automatically detects the following audio filenames:
- `song.opus`, `guitar.opus` (Opus 48kHz — recommended for fastest load times)
- `song.ogg`, `guitar.ogg` (Ogg Vorbis 44.1kHz or 48kHz)
- `song.mp3`, `guitar.mp3` (MP3 44.1kHz or 48kHz)

> **Legal Notice**: Do not distribute copyrighted songs or commercial audio tracks with this repository. Users must supply their own legitimate `.chart` and audio files.

---

## Building from Source

The build pipeline uses the official PSP toolchain container (`pspdev/pspdev:latest`), so no manual compiler installation is needed.

### Prerequisites
- Docker engine installed and running.

### Compile `EBOOT.PBP`
Run the provided Docker build script from the repository root:

```bash
./docker-build.sh
```

This compiles all C++ sources with `psp-g++ -std=c++17 -O2 -G0`, links static libraries (`libopusfile`, `libopus`, `libogg`, `libpspaudio`, `libpspgu`, `libpsprtc`), encrypts the binary with `PrxEncrypter` for real hardware compatibility (`BUILD_PRX = 1`, `ENCRYPT = 1`), and packages the final signed **`EBOOT.PBP`**.

### Host Parser Test Tool
To verify chart parsing on Linux/macOS without running the emulator:

```bash
make -f Makefile.native run
```

---

## Installation & Deployment

### Real Hardware (PSP 1000 / 2000 / 3000 / Street / Go)
1. Connect your PSP to your PC via USB or insert your Memory Stick into a card reader.
2. Copy the game folder containing `EBOOT.PBP` and your `songs/` folder to:
   ```text
   ms0:/PSP/GAME/cloneHero/
   ├── EBOOT.PBP
   └── songs/
       └── <Song Folders Here>/
   ```
3. Disconnect USB and launch **PSP Hero** from **Game > Memory Stick** on the PSP XMB.

### PPSSPP Emulator (Windows / Linux / Android / macOS)
1. Navigate to your PPSSPP memory stick folder (e.g. `memstick/PSP/GAME/`).
2. Create a folder named `cloneHero` and place `EBOOT.PBP` and `songs/` inside.
3. Open PPSSPP and click the **PSP Hero** icon to play.

---

## License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

### Third-Party Libraries
- **stb_vorbis** & **stb_image**: Public Domain / MIT by Sean Barrett.
- **minimp3**: CC0 1.0 Universal (Public Domain) by Lieven van der Heide.
- **font8x8**: Public Domain bitmap font by Daniel Hepper.
- **libopus** & **libopusfile**: BSD 3-Clause License by Xiph.Org Foundation and Skype Limited.
- **PSPSDK**: BSD 3-Clause License by the PSPDEV team.
