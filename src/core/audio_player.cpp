#include "audio_player.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

#ifdef __PSP__
#include <pspkernel.h>
#include <pspaudio.h>
#include <psprtc.h>
#include <opus/opusfile.h>
#endif

AudioPlayer::AudioPlayer()
    : m_loaded(false)
    , m_playing(false)
    , m_thread_running(false)
    , m_seek_requested(false)
    , m_seek_target_seconds(0.0)
    , m_volume(PSP_AUDIO_VOLUME_MAX)
    , m_opus_file(nullptr)
    , m_audio_channel(-1)
    , m_use_src_channel(false)
    , m_sample_rate(AUDIO_SAMPLE_RATE_48K)
    , m_thread_id(-1)
    , m_current_samples(0)
    , m_total_samples(0)
    , m_total_duration(0.0)
    , m_ring_buffer(nullptr)
    , m_ring_write_head(0)
    , m_ring_read_tail(0)
    , m_ring_count(0)
    , m_decode_avail_48k(0)
    , m_resample_src_pos(0.0)
    , m_last_block_tick(0)
    , m_playback_start_tick(0)
    , m_paused_tick(0)
    , m_clock_offset(0.0)
    , m_use_fallback_clock(true)
{
    m_ring_buffer = reinterpret_cast<int16_t*>(std::malloc(RING_BUFFER_FRAMES * 2 * sizeof(int16_t)));
    if (m_ring_buffer) {
        std::memset(m_ring_buffer, 0, RING_BUFFER_FRAMES * 2 * sizeof(int16_t));
    }
}

AudioPlayer::~AudioPlayer() {
    shutdown();
    if (m_ring_buffer) {
        std::free(m_ring_buffer);
        m_ring_buffer = nullptr;
    }
}

int AudioPlayer::audioThreadFunc(unsigned int args, void* argp) {
    (void)args;
    AudioPlayer* player = *reinterpret_cast<AudioPlayer**>(argp);
    if (player) {
        player->runAudioLoop();
    }
    return 0;
}

void AudioPlayer::decodeChunk() {
#ifdef __PSP__
    if (!m_opus_file || !m_ring_buffer) return;
    OggOpusFile* of = reinterpret_cast<OggOpusFile*>(m_opus_file);

    if (m_use_src_channel) {
        // Native 48kHz decoding: decode directly into ring buffer without any software resampling
        int space = RING_BUFFER_FRAMES - m_ring_count;
        if (space < 1024) return;

        // Decode up to 1024 frames (or available contiguous space in ring buffer)
        int contiguous = RING_BUFFER_FRAMES - m_ring_write_head;
        int to_read = std::min(1024, std::min(space, contiguous));

        int frames_read = op_read_stereo(of, m_ring_buffer + m_ring_write_head * 2, to_read * 2);
        if (frames_read > 0) {
            m_ring_write_head = (m_ring_write_head + frames_read) % RING_BUFFER_FRAMES;
            m_ring_count += frames_read;
        }
    } else {
        // Fallback: 48kHz to 44.1kHz linear resampling
        const double ratio = 48000.0 / 44100.0;
        int space = RING_BUFFER_FRAMES - m_ring_count;
        if (space < 1024) return;

        if (m_decode_avail_48k <= 1024) {
            int base_idx = static_cast<int>(m_resample_src_pos);
            int remain = m_decode_avail_48k - base_idx;
            if (remain < 0) remain = 0;
            if (remain > 4) remain = 4;

            for (int i = 0; i < remain; ++i) {
                m_decode_buf_48k[i * 2]     = m_decode_buf_48k[(base_idx + i) * 2];
                m_decode_buf_48k[i * 2 + 1] = m_decode_buf_48k[(base_idx + i) * 2 + 1];
            }
            m_resample_src_pos -= base_idx;
            if (m_resample_src_pos < 0.0) m_resample_src_pos = 0.0;

            int frames_to_read = 2048 - remain;
            int frames_read = op_read_stereo(of, m_decode_buf_48k + remain * 2, frames_to_read * 2);
            if (frames_read > 0) {
                m_decode_avail_48k = remain + frames_read;
            }
        }

        // Resample available 48kHz samples
        while (static_cast<int>(m_resample_src_pos) + 1 < m_decode_avail_48k &&
               m_ring_count < (RING_BUFFER_FRAMES - 64)) {
            int idx = static_cast<int>(m_resample_src_pos);
            float frac = static_cast<float>(m_resample_src_pos - idx);

            int16_t l = static_cast<int16_t>((1.0f - frac) * m_decode_buf_48k[idx * 2] +
                                            frac * m_decode_buf_48k[(idx + 1) * 2]);
            int16_t r = static_cast<int16_t>((1.0f - frac) * m_decode_buf_48k[idx * 2 + 1] +
                                            frac * m_decode_buf_48k[(idx + 1) * 2 + 1]);

            m_ring_buffer[m_ring_write_head * 2]     = l;
            m_ring_buffer[m_ring_write_head * 2 + 1] = r;

            m_ring_write_head = (m_ring_write_head + 1) % RING_BUFFER_FRAMES;
            m_ring_count++;

            m_resample_src_pos += ratio;
        }
    }
#endif
}

void AudioPlayer::runAudioLoop() {
#ifdef __PSP__
    // Try reserving native 48kHz SRC channel first
    m_audio_channel = sceAudioSRCChReserve(AUDIO_BLOCK_SAMPLES, AUDIO_SAMPLE_RATE_48K, 2);
    if (m_audio_channel >= 0) {
        m_use_src_channel = true;
        m_sample_rate = AUDIO_SAMPLE_RATE_48K;
    } else {
        // Fallback to standard 44.1kHz channel
        m_audio_channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, AUDIO_BLOCK_SAMPLES, PSP_AUDIO_FORMAT_STEREO);
        m_use_src_channel = false;
        m_sample_rate = AUDIO_SAMPLE_RATE_44K;
        if (m_audio_channel < 0) {
            m_use_fallback_clock = true;
            return;
        }
    }

    int16_t out_block[AUDIO_BLOCK_SAMPLES * 2];
    int16_t silence_block[AUDIO_BLOCK_SAMPLES * 2];
    std::memset(silence_block, 0, sizeof(silence_block));

    OggOpusFile* of = reinterpret_cast<OggOpusFile*>(m_opus_file);

    // Initial pre-buffer (~8,192 frames, roughly 170ms)
    for (int i = 0; i < 8 && m_thread_running; ++i) {
        decodeChunk();
    }

    while (m_thread_running) {
        if (m_seek_requested) {
            int64_t target_48k = static_cast<int64_t>(m_seek_target_seconds * AUDIO_SAMPLE_RATE_48K);
            if (target_48k < 0) target_48k = 0;
            if (target_48k > m_total_samples) target_48k = m_total_samples;

            op_pcm_seek(of, target_48k);

            // Flush ring buffer
            m_ring_write_head = 0;
            m_ring_read_tail = 0;
            m_ring_count = 0;
            m_decode_avail_48k = 0;
            m_resample_src_pos = 0.0;
            m_current_samples = static_cast<int64_t>(m_seek_target_seconds * m_sample_rate);
            sceRtcGetCurrentTick(&m_last_block_tick);
            m_seek_requested = false;

            // Pre-fill ring buffer
            for (int i = 0; i < 8 && m_thread_running; ++i) {
                decodeChunk();
            }
        }

        // Maintain ring buffer (~8k to 12k frames)
        if (m_ring_count < 12288) {
            decodeChunk();
        }

        if (!m_playing) {
            // Output silence block when paused
            if (m_use_src_channel) {
                sceAudioSRCOutputBlocking(0, silence_block);
            } else {
                sceAudioOutputPannedBlocking(m_audio_channel, 0, 0, silence_block);
            }
            sceKernelDelayThread(2000); // 2ms sleep to yield CPU
            continue;
        }

        if (m_ring_count >= AUDIO_BLOCK_SAMPLES) {
            // Copy audio block from ring buffer
            for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
                int read_idx = (m_ring_read_tail + i) % RING_BUFFER_FRAMES;
                out_block[i * 2]     = m_ring_buffer[read_idx * 2];
                out_block[i * 2 + 1] = m_ring_buffer[read_idx * 2 + 1];
            }
            m_ring_read_tail = (m_ring_read_tail + AUDIO_BLOCK_SAMPLES) % RING_BUFFER_FRAMES;
            m_ring_count -= AUDIO_BLOCK_SAMPLES;

            // Update tick stamp right before output blocking
            sceRtcGetCurrentTick(&m_last_block_tick);

            // Output block to PSP hardware DAC
            if (m_use_src_channel) {
                sceAudioSRCOutputBlocking(m_volume, out_block);
            } else {
                sceAudioOutputPannedBlocking(m_audio_channel, m_volume, m_volume, out_block);
            }
            m_current_samples += AUDIO_BLOCK_SAMPLES;
        } else {
            // Underrun protection: decode one chunk and output silence block
            decodeChunk();
            if (m_use_src_channel) {
                sceAudioSRCOutputBlocking(0, silence_block);
            } else {
                sceAudioOutputPannedBlocking(m_audio_channel, 0, 0, silence_block);
            }
        }
    }

    // Clean channel release
    if (m_audio_channel >= 0) {
        if (m_use_src_channel) {
            sceAudioSRCChRelease();
        } else {
            sceAudioChRelease(m_audio_channel);
        }
        m_audio_channel = -1;
    }
#endif
}

bool AudioPlayer::init(const std::string& filepath) {
    // ALWAYS cleanly shutdown any previously running audio thread or channel first!
    shutdown();

    m_loaded = false;
    m_use_fallback_clock = true;
    m_current_samples = 0;
    m_total_samples = 0;
    m_total_duration = 0.0;
    m_clock_offset = 0.0;
    m_sample_rate = AUDIO_SAMPLE_RATE_48K;

#ifdef __PSP__
    sceRtcGetCurrentTick(&m_playback_start_tick);
    sceRtcGetCurrentTick(&m_last_block_tick);
#endif

    if (filepath.empty()) {
        return false;
    }

#ifdef __PSP__
    int error = 0;
    OggOpusFile* of = op_open_file(filepath.c_str(), &error);
    if (!of) {
        std::string alt = "song/" + filepath;
        of = op_open_file(alt.c_str(), &error);
    }

    if (of) {
        // Disable dithering for maximum MIPS decoding throughput
        op_set_dither_enabled(of, 0);

        m_opus_file = of;
        m_total_samples = op_pcm_total(of, -1);
        m_total_duration = static_cast<double>(m_total_samples) / AUDIO_SAMPLE_RATE_48K;
        m_current_samples = 0;
        m_loaded = true;
        m_use_fallback_clock = false;

        // Reset ring buffer heads
        m_ring_write_head = 0;
        m_ring_read_tail = 0;
        m_ring_count = 0;
        m_decode_avail_48k = 0;
        m_resample_src_pos = 0.0;

        // Start dedicated playback thread with priority 0x18
        m_thread_running = true;
        AudioPlayer* self = this;
        m_thread_id = sceKernelCreateThread("audio_thread", audioThreadFunc, 0x18, 0x10000, 0, nullptr);
        if (m_thread_id >= 0) {
            sceKernelStartThread(m_thread_id, sizeof(void*), &self);
        } else {
            m_use_fallback_clock = true;
        }
    }
#endif

    m_playing = false;
    return m_loaded;
}

void AudioPlayer::play() {
    if (m_playing) return;
    m_playing = true;

#ifdef __PSP__
    sceRtcGetCurrentTick(&m_playback_start_tick);
    sceRtcGetCurrentTick(&m_last_block_tick);
#endif
}

void AudioPlayer::pause() {
    if (!m_playing) return;
    m_playing = false;

#ifdef __PSP__
    uint64_t now;
    sceRtcGetCurrentTick(&now);
    uint32_t freq = sceRtcGetTickResolution();
    double elapsed = static_cast<double>(now - m_playback_start_tick) / freq;
    m_clock_offset += elapsed;
#endif
}

void AudioPlayer::togglePause() {
    if (m_playing) pause();
    else play();
}

void AudioPlayer::seek(double seconds) {
    if (seconds < 0.0) seconds = 0.0;
    if (m_total_duration > 0.0 && seconds > m_total_duration) {
        seconds = m_total_duration;
    }

    m_seek_target_seconds = seconds;
    m_seek_requested = true;
    m_clock_offset = seconds;

#ifdef __PSP__
    sceRtcGetCurrentTick(&m_playback_start_tick);
    sceRtcGetCurrentTick(&m_last_block_tick);
#endif
}

void AudioPlayer::setVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > PSP_AUDIO_VOLUME_MAX) volume = PSP_AUDIO_VOLUME_MAX;
    m_volume = volume;
}

double AudioPlayer::getTimeSeconds() const {
    if (m_loaded && !m_use_fallback_clock) {
#ifdef __PSP__
        if (!m_playing) {
            return static_cast<double>(m_current_samples) / m_sample_rate;
        }

        // Sub-block hardware clock interpolation
        uint64_t now;
        sceRtcGetCurrentTick(&now);
        uint32_t freq = sceRtcGetTickResolution();
        double sub_block = 0.0;
        if (m_last_block_tick > 0 && now >= m_last_block_tick) {
            sub_block = static_cast<double>(now - m_last_block_tick) / freq;
            double max_sub = static_cast<double>(AUDIO_BLOCK_SAMPLES) / m_sample_rate;
            if (sub_block > max_sub) sub_block = max_sub;
        }

        return (static_cast<double>(m_current_samples) / m_sample_rate) + sub_block;
#else
        return static_cast<double>(m_current_samples) / m_sample_rate;
#endif
    }

#ifdef __PSP__
    if (!m_playing) {
        return m_clock_offset;
    }
    uint64_t now;
    sceRtcGetCurrentTick(&now);
    uint32_t freq = sceRtcGetTickResolution();
    double elapsed = static_cast<double>(now - m_playback_start_tick) / freq;
    return m_clock_offset + elapsed;
#else
    return m_clock_offset;
#endif
}

void AudioPlayer::shutdown() {
    m_playing = false;
    m_thread_running = false;

#ifdef __PSP__
    if (m_thread_id >= 0) {
        sceKernelWaitThreadEnd(m_thread_id, nullptr);
        sceKernelDeleteThread(m_thread_id);
        m_thread_id = -1;
    }

    if (m_opus_file) {
        op_free(reinterpret_cast<OggOpusFile*>(m_opus_file));
        m_opus_file = nullptr;
    }
#endif

    m_loaded = false;
}
