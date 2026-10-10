---
name: audio-pipeline
description: >-
  Manage, inspect, convert, and test audio tracks for PSP Hero. Covers Opus, Vorbis (Ogg), and MP3 decoding, sample rate matching, and audio synchronization. Use when adding audio, debugging playback, or optimizing audio performance.
---

# PSP Hero Audio Pipeline & Codec Guide

This skill details the audio subsystem, codec support, sample rate handling, and synchronization in PSP Hero.

## 1. Supported Audio Formats
PSP Hero supports three major compressed audio formats natively:

1. **Ogg Opus (`.opus`)**:
   - Decoded via `libopusfile`.
   - Native sample rate: 48,000 Hz stereo.
   - Recommended for new songs due to high audio quality at low bitrates (~64-96 kbps).
   - Dithering is explicitly disabled (`op_set_dither_enabled(of, 0)`) to eliminate float math on PSP MIPS R4000.

2. **Ogg Vorbis (`.ogg`)**:
   - Decoded via **stb_vorbis** (`stb_vorbis.c`, public domain Vorbis decoder).
   - Clean 16-bit stereo decoding, replacing buggy SDK Tremor implementation.
   - Native sample rates: 44,100 Hz or 48,000 Hz stereo/mono.

3. **MPEG Audio Layer 3 (`.mp3`)**:
   - Decoded via **minimp3** (`minimp3_ex.h`).
   - Native sample rates: 44,100 Hz or 48,000 Hz.
   - Header-only, embedded directly in `src/core/`.

---

## 2. Hardware Audio Channel & Sample Rates
The PSP audio hardware DAC operates natively at **44,100 Hz**:
- **44.1 kHz Audio (All standard Vorbis `.ogg` & MP3 `.mp3`)**:
  - Uses the standard PSP hardware channel:
    `sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, AUDIO_BLOCK_SAMPLES, PSP_AUDIO_FORMAT_STEREO)`
  - Outputs directly to DAC via: `sceAudioOutputPannedBlocking(channel, volume, volume, buffer)`
  - Zero conversion overhead, perfect bit-accurate playback on both hardware and PPSSPP.
- **48.0 kHz Audio (Opus `.opus` & 48kHz streams)**:
  - Uses the hardware Sample Rate Converter (SRC) channel:
    `sceAudioSRCChReserve(AUDIO_BLOCK_SAMPLES, 48000, 2)`
  - Outputs via: `sceAudioSRCOutputBlocking(volume, buffer)`
  - Downsamples 48kHz -> 44.1kHz in hardware.
  - *Note*: Never pass 44.1kHz into `sceAudioSRCChReserve`, as the SRC is designed for 48k downsampling and causes distortion or wrong pitch in emulators and hardware.

---

## 3. Audio Thread Architecture & Buffer Management
- Audio playback runs on a dedicated high-priority kernel thread (priority `0x18`, stack `64KB`).
- Ring buffer holds **16,384 stereo frames** (~371ms at 44.1kHz / ~341ms at 48kHz).
- **Vorbis & MP3 Packet Looping**: Tremor's `ov_read` and `mp3dec_ex_read` decode variable-length packets (e.g. 128-512 samples per call). `decodeChunk()` **MUST loop** until all 1,024 requested frames are filled or EOF is reached. Without looping, buffer starvation occurs, causing stuttering and silence gap output.
- When paused or when underrun occurs, the thread outputs silence blocks to maintain audio DMA timing.
- **CRITICAL**: Before loading a new song or returning to menus, `AudioPlayer::shutdown()` MUST be called to stop the thread and release the hardware channel (`sceAudioSRCChRelease()` or `sceAudioChRelease()`). Failure to do so deadlocks the PSP hardware.

---

## 4. Sub-Block Microsecond Synchronization
To ensure 60 FPS smooth highway scrolling without audio block jitter:
- `sceRtcGetCurrentTick(&m_last_block_tick)` records the exact hardware RTC tick when an audio block is written to DAC.
- `AudioPlayer::getTimeSeconds()` interpolates the microsecond elapsed time between blocks using `sceRtcGetTickResolution()`, preventing 21ms stepped jumps.
