# PSP Hero (Clone Hero for Sony PSP) - Agent Guidelines

## 1. Project Overview & Tech Stack
- **Project**: PSP Hero is a full-featured rhythm game for the Sony PlayStation Portable (PSP) inspired by Clone Hero and Guitar Hero.
- **Language**: C++17 (compiled with `psp-g++`, `-std=c++17 -fno-exceptions -fno-rtti`).
- **Platform**: Sony PlayStation Portable (MIPS R4000, 333 MHz) and PPSSPP emulator.
- **Audio Engines**:
  - `libopusfile` / `libopus` for Opus audio (`.opus`, 48,000 Hz).
  - `stb_vorbis` for Ogg Vorbis audio (`.ogg`, robust embedded 16-bit stereo decoding replacing buggy SDK Tremor).
  - `minimp3_ex.h` for MPEG Audio Layer 3 (`.mp3`, public-domain single header decoder).
  - PSP hardware Sample Rate Converter channel (`sceAudioSRCChReserve`) for 44.1 kHz / 48 kHz native playback.
  - Dedicated hardware sound effects channel with synthetic low-latency PCM tables (`sound_effects_data.h`).
- **Rendering & Math**:
  - `pspgu` and `pspgum` for hardware-accelerated 3D projection and 2D primitive rendering.
  - `psprtc` for high-precision microsecond sub-block audio-visual synchronization.
- **Build System**:
  - Docker container `pspdev/pspdev:latest` via `docker-build.sh`.
  - Produces signed and encrypted `EBOOT.PBP` (`BUILD_PRX = 1`, `ENCRYPT = 1`) compatible with both CFW/OFW real hardware and emulators.

---

## 2. Core Architecture & Invariants

### A. UI & Menu Invariants
1. **Main Menu**:
   - MUST NOT have a "Quit" or "Exit" button (following standard PSP OS guidelines where the hardware HOME/PS button handles quitting to XMB).
   - Menu items: Quick Play, Song Select, Options. Quick Play randomly selects a song from the library.
2. **Pause Menu (In-Game)**:
   - MUST have "Resume", "Restart", and "Quit to Songs" options.
   - Resuming from pause triggers an animated **3-2-1 countdown** with audio tick cues before note travel resumes.
3. **Song Browser**:
   - Primary songs directory is `songs/` (with automatic fallback to `song/`).
   - Instant response with wrap-around navigation (Up at top wraps to bottom, Down at bottom wraps to top).
   - Left / Right and L / R triggers page up / page down by 5 songs.
   - Album covers are cached in memory via `stb_image` to avoid per-frame disk I/O.
4. **Difficulty Selection**:
   - Dynamic: automatically queries the parsed `.chart` and only displays difficulties that actually exist in the song (e.g. if only Expert exists, Easy/Medium/Hard are omitted).
5. **Button Remapper**:
   - Any game action can be freely bound to any PSP button.
   - The **Circle button** is a regular assignable button and can be bound to frets or actions.
   - DO NOT treat Circle as a cancel/back action while a remapping prompt is active. Triangle is used to exit the menu.

### B. Gameplay & Scoring Invariants
1. **Autoplay Mode**:
   - Autoplay demo mode is toggled via SELECT.
   - High scores are strictly disabled during Autoplay (`session.save_high_scores = !g_config.autoplay`).
2. **Sustain Notes**:
   - Notes with duration > 0.05s trigger sustain mode.
   - Sustains require the player to hold the corresponding fret until note end. Letting go early drops the sustain and stops awarding points.
3. **Star Power**:
   - Notes transform into electric cyan (`COLOR_CYAN`) when Star Power is active.
   - Highway rails glow cyan and multiplier doubles (up to 8x).
4. **Hit Windows & Misfires**:
   - Hit window is 100ms ($\pm 0.10s$).
   - Notes require a fresh press in Tap Mode (holding frets does not auto-strike upcoming notes).
   - Misfires/overstrums and missed notes trigger the custom guitar pick scratch SFX (debounced at 100ms).

### C. Hardware & Stability Invariants
1. **Hardware Encryption (`BUILD_PRX = 1`, `ENCRYPT = 1`)**:
   - The PSP requires signed/encrypted PRX executables in `EBOOT.PBP` to run on real hardware without triggering error 80020148 or "Data is corrupted".
   - Never disable `BUILD_PRX = 1` or `ENCRYPT = 1` in `Makefile`.
2. **Audio Concurrency & Thread Cleanliness**:
   - State switches and `AudioPlayer::init()` MUST call `AudioPlayer::shutdown()` first to cleanly terminate audio threads and release reserved hardware channels (`sceAudioSRCChRelease` / `sceAudioChRelease`).
   - Failure to release the audio channel or wait for thread exit causes hardware deadlocks and freezes the PSP.
3. **Performance & Memory**:
   - Disable Opus dithering (`op_set_dither_enabled(of, 0)`) to prevent float overhead on MIPS R4000.
   - Use integer arithmetic where possible.
   - Use early-exit bounds when looping over chart notes during rendering and hit-detection.
   - User settings are saved to `config.dat` and high scores to `scores.dat`.

---

## 3. Directory Layout
```text
cloneHero/
├── .agents/               # Agent configuration, rules, and skills
│   ├── AGENTS.md          # This guidelines document
│   └── skills/            # Procedural skills for build, chart, audio, deploy
├── src/
│   ├── core/              # Engine core (chart parsing, audio, song manager, config, SFX)
│   ├── render/            # Graphics and font rendering (Gu/Gum, stb_image, font8x8)
│   ├── game/              # Highway perspective projection, note gem rendering, gameplay logic
│   ├── ui/                # UI menus, cards, difficulty select, pause countdown, HUD
│   └── main.cpp           # Game initialization, main loop, state machine
├── songs/                 # Song library (folders containing charts, audio, covers)
├── tools/                 # Conversion and testing tools (mid2chart.py, test_parser)
├── ICON0.PNG              # 144x80 XMB game icon
├── Makefile               # PSPSDK Makefile for PRX and EBOOT generation
├── Makefile.native        # Host testing Makefile
├── docker-build.sh        # Script compiling EBOOT via pspdev/pspdev:latest
├── LICENSE                # MIT License
└── README.md              # Project documentation
```

---

## 4. Git & Commits
- Changes should be well-tested and committed to `master` with descriptive, concise commit messages.
- Always verify that `./docker-build.sh` produces a valid `EBOOT.PBP` before completing tasks.
