# PSP Hero (Clone Hero for Sony PSP) - Agent Guidelines

## 1. Project Overview & Tech Stack
- **Project**: PSP Hero is a full-featured rhythm game for the Sony PlayStation Portable (PSP) inspired by Clone Hero and Guitar Hero.
- **Language**: C++17 (compiled with `psp-g++`, `-std=c++17 -fno-exceptions -fno-rtti`).
- **Platform**: Sony PlayStation Portable (MIPS R4000, 333 MHz) and PPSSPP emulator.
- **Audio Engines**:
  - `libopusfile` / `libopus` for Opus audio (`.opus`, 48,000 Hz).
  - `stb_vorbis` for Ogg Vorbis audio (`.ogg`, robust 16-bit stereo decoding replacing buggy Tremor).
  - `minimp3_ex.h` for MPEG Audio Layer 3 (`.mp3`, public-domain single header decoder).
  - PSP hardware Sample Rate Converter channel (`sceAudioSRCChReserve`) for 44.1 kHz / 48 kHz native playback.
- **Rendering & Math**:
  - `pspgu` and `pspgum` for hardware-accelerated 3D projection and 2D primitive rendering.
  - `psprtc` for high-precision microsecond sub-block audio-visual synchronization.
- **Build System**:
  - Docker container `pspdev/pspdev:latest` via `docker-build.sh`.
  - Produces signed and encrypted `EBOOT.PBP` compatible with both CFW/OFW real hardware and emulators.

---

## 2. Critical Invariants & Rules

### A. Menu UI Invariants
1. **Main Menu**: MUST NOT have a "Quit" or "Exit" button. The user requested this explicitly.
2. **Pause Menu (In-Game)**: MUST have a "Quit to Songs" / "Quit" button to allow backing out of gameplay back to the song selection screen.
3. **Button Remapper**:
   - The **Circle button** is a regular button and can be freely bound to any game action (Green, Red, Yellow, Blue, Orange, Strum Up, Strum Down, Star Power, Pause).
   - DO NOT treat Circle as a cancel/back action while a remapping prompt is active.
   - Triangle is used to exit the Button Remapper menu.
4. **Song Selector**:
   - Primary songs directory is `songs/` (with automatic fallback to `song/`).
   - Each song resides in its own subfolder containing `notes.chart` (or `chart.chart`), audio file (`song.*` or `guitar.*`), and album cover (`album.png` or `album.jpg`).
   - Supports difficulty selection (Easy, Medium, Hard, Expert) before gameplay starts.

### B. Hardware & Stability Invariants
1. **Hardware Encryption (`BUILD_PRX = 1`, `ENCRYPT = 1`)**:
   - The PSP requires signed/encrypted PRX executables in `EBOOT.PBP` to run on real hardware without triggering "The game could not be started. (80020148)" or "Data is corrupted".
   - Never disable `BUILD_PRX = 1` or `ENCRYPT = 1` in `Makefile`.
2. **Audio Concurrency & Thread Cleanliness**:
   - `AudioPlayer::init()` and state switches MUST call `AudioPlayer::shutdown()` first to cleanly stop running audio threads and release reserved hardware channels (`sceAudioSRCChRelease` / `sceAudioChRelease`).
   - Failure to release the audio channel or wait for thread exit causes hardware thread deadlocks and freezes the PSP.
3. **Performance & Memory**:
   - Disable Opus dithering (`op_set_dither_enabled(of, 0)`) to prevent float overhead on MIPS R4000.
   - Use integer arithmetic where possible (e.g. Tremor fixed-point Vorbis).
   - Album covers must be cached or thumbnail-drawn without reloading textures from disk every frame.
   - Use early-exit bounds when looping over chart notes during rendering and hit-detection.

---

## 3. Directory Layout
```text
cloneHero/
├── .agents/               # Agent configuration, rules, and skills
│   ├── AGENTS.md          # This guidelines document
│   └── skills/            # Procedural skills for build, chart, audio, deploy
├── src/
│   ├── core/              # Engine core (chart parsing, audio, song manager, config)
│   ├── render/            # Graphics and font rendering (Gu/Gum, stb_image, font8x8)
│   ├── game/              # Highway perspective projection, note gem rendering, gameplay logic
│   ├── ui/                # Prototype-matching UI menus, cards, and cursors
│   └── main.cpp           # Game initialization, main loop, state machine
├── songs/                 # Song library (folders containing charts, audio, covers)
├── tools/                 # Conversion and testing tools (mid2chart.py, test_parser)
├── Makefile               # PSPSDK Makefile for PRX and EBOOT generation
├── Makefile.native        # Host testing Makefile
└── docker-build.sh        # Script compiling EBOOT via pspdev/pspdev:latest
```

---

## 4. Git & Commits
- Changes should be well-tested and committed to `master` with descriptive, concise commit messages.
- Always verify that `./docker-build.sh` produces a valid `EBOOT.PBP` before completing tasks.
