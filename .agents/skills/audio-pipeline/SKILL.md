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
   - Decoded via **Tremor** (`libvorbisidec`, integer fixed-point Vorbis decoder).
   - Native sample rates: 44,100 Hz or 48,000 Hz stereo/mono.
   - Zero floating-point CPU overhead.

3. **MPEG Audio Layer 3 (`.mp3`)**:
   - Decoded via **minimp3** (`minimp3_ex.h`).
   - Native sample rates: 44,100 Hz or 48,000 Hz.
   - Header-only, embedded directly in `src/core/`.

---

## 2. Hardware Audio Channel & Sample Rates
The PSP audio hardware provides a specialized Sample Rate Converter (SRC) channel:
```cpp
m_audio_channel = sceAudioSRCChReserve(AUDIO_BLOCK_SAMPLES, m_sample_rate, 2);
```
- For **48 kHz** audio (Opus): `sceAudioSRCChReserve(1024, 48000, 2)` outputs 48 kHz directly.
- For **44.1 kHz** audio (Vorbis / MP3): `sceAudioSRCChReserve(1024, 44100, 2)` outputs 44.1 kHz natively in hardware without any software resampling burden.
- Output function: `sceAudioSRCOutputBlocking(m_volume, out_block);`.

---

## 3. Audio Thread Architecture & Deadlock Prevention
- Audio playback runs on a dedicated high-priority kernel thread (priority `0x18`, stack `64KB`).
- Ring buffer holds **16,384 stereo frames** (~341ms at 48kHz).
- Audio is decoded in chunks of 1,024 frames into the ring buffer.
- When paused or when underrun occurs, the thread outputs silence blocks to maintain audio DMA timing.
- **CRITICAL**: Before loading a new song or returning to menus, `AudioPlayer::shutdown()` MUST be called to stop the thread and release the hardware channel via `sceAudioSRCChRelease()`. Failure to do so deadlocks the PSP hardware.

---

## 4. Sub-Block Microsecond Synchronization
To ensure 60 FPS smooth highway scrolling without audio block jitter:
- `sceRtcGetCurrentTick(&m_last_block_tick)` records the exact hardware RTC tick when an audio block is written to DAC.
- `AudioPlayer::getTimeSeconds()` interpolates the microsecond elapsed time between blocks using `sceRtcGetTickResolution()`, preventing 21ms stepped jumps.
