#include "audio_player.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

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
    , m_fallback_start_tick(0)
    , m_fallback_paused_tick(0)
    , m_fallback_time_offset(0.0)
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

void AudioPlayer::fillRingBuffer() {
#ifdef __PSP__
    if (!m_opus_file || !m_ring_buffer) return;
    OggOpusFile* of = reinterpret_cast<OggOpusFile*>(m_opus_file);

    // Resampling ratio: 48000 Hz source to 44100 Hz output
    const double ratio = 48000.0 / 44100.0;

    // Fill ring buffer up to near capacity
    while (m_ring_count < (RING_BUFFER_FRAMES - 2048) && m_thread_running) {
        int needed_src_idx = static_cast<int>(m_resample_src_pos) + 2;

        if (needed_src_idx >= m_decode_avail_48k) {
            // Shift unconsumed tail frames to start of decode buffer
            int base_idx = static_cast<int>(m_resample_src_pos);
            int remain = m_decode_avail_48k - base_idx;
            if (remain < 0) remain = 0;
            if (remain > 4) remain = 4; // Safety clamp

            for (int i = 0; i < remain; ++i) {
                m_decode_buf_48k[i * 2]     = m_decode_buf_48k[(base_idx + i) * 2];
                m_decode_buf_48k[i * 2 + 1] = m_decode_buf_48k[(base_idx + i) * 2 + 1];
            }
            m_resample_src_pos -= base_idx;
            if (m_resample_src_pos < 0.0) m_resample_src_pos = 0.0;

            // Read new 48kHz audio from Opus
            // Note: op_read_stereo takes the buffer capacity in TOTAL values (2 * frames)
            int frames_to_read = 2048 - remain;
            int frames_read = op_read_stereo(of, m_decode_buf_48k + remain * 2, frames_to_read * 2);

            if (frames_read > 0) {
                m_decode_avail_48k = remain + frames_read;
            } else {
                // End of file or read error
                m_decode_avail_48k = remain;
                break;
            }
        }

        // Resample available 48kHz frames to 44.1kHz and write to ring buffer
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
    // Reserve native PSP hardware stereo audio channel at 44,100 Hz
    m_audio_channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, AUDIO_BLOCK_SAMPLES, PSP_AUDIO_FORMAT_STEREO);
    if (m_audio_channel < 0) {
        m_use_fallback_clock = true;
        return;
    }

    int16_t out_block[AUDIO_BLOCK_SAMPLES * 2];
    int16_t silence_block[AUDIO_BLOCK_SAMPLES * 2];
    std::memset(silence_block, 0, sizeof(silence_block));

    OggOpusFile* of = reinterpret_cast<OggOpusFile*>(m_opus_file);

    // Initial pre-buffer before playback begins
    fillRingBuffer();

    while (m_thread_running) {
        if (m_seek_requested) {
            int64_t target_48k = static_cast<int64_t>(m_seek_target_seconds * OPUS_SAMPLE_RATE);
            if (target_48k < 0) target_48k = 0;
            if (target_48k > m_total_samples) target_48k = m_total_samples;

            op_pcm_seek(of, target_48k);

            // Flush ring buffer and resampler
            m_ring_write_head = 0;
            m_ring_read_tail = 0;
            m_ring_count = 0;
            m_decode_avail_48k = 0;
            m_resample_src_pos = 0.0;
            m_current_samples = static_cast<int64_t>(m_seek_target_seconds * NATIVE_SAMPLE_RATE);
            m_seek_requested = false;

            // Pre-fill ring buffer at new position
            fillRingBuffer();
        }

        // Keep ring buffer well stocked
        fillRingBuffer();

        if (!m_playing) {
            // Output silence when paused to keep audio hardware ready
            sceAudioOutputPannedBlocking(m_audio_channel, 0, 0, silence_block);
            sceKernelDelayThread(1000); // 1ms sleep
            continue;
        }

        if (m_ring_count >= AUDIO_BLOCK_SAMPLES) {
            // Pull a block of 1024 44.1kHz stereo samples from ring buffer
            for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
                int read_idx = (m_ring_read_tail + i) % RING_BUFFER_FRAMES;
                out_block[i * 2]     = m_ring_buffer[read_idx * 2];
                out_block[i * 2 + 1] = m_ring_buffer[read_idx * 2 + 1];
            }
            m_ring_read_tail = (m_ring_read_tail + AUDIO_BLOCK_SAMPLES) % RING_BUFFER_FRAMES;
            m_ring_count -= AUDIO_BLOCK_SAMPLES;

            // Output directly to PSP DAC
            sceAudioOutputPannedBlocking(m_audio_channel, m_volume, m_volume, out_block);
            m_current_samples += AUDIO_BLOCK_SAMPLES;
        } else {
            // Underrun protection (output silence block while refilling)
            sceAudioOutputPannedBlocking(m_audio_channel, 0, 0, silence_block);
        }
    }

    if (m_audio_channel >= 0) {
        sceAudioChRelease(m_audio_channel);
        m_audio_channel = -1;
    }
#endif
}

bool AudioPlayer::init(const std::string& filepath) {
    m_loaded = false;
    m_use_fallback_clock = true;

#ifdef __PSP__
    int error = 0;
    OggOpusFile* of = op_open_file(filepath.c_str(), &error);
    if (!of) {
        // Try fallback paths
        std::string alt1 = "song/" + filepath;
        of = op_open_file(alt1.c_str(), &error);
    }

    if (of) {
        m_opus_file = of;
        m_total_samples = op_pcm_total(of, -1);
        m_total_duration = static_cast<double>(m_total_samples) / OPUS_SAMPLE_RATE;
        m_current_samples = 0;
        m_loaded = true;
        m_use_fallback_clock = false;

        // Reset ring buffer & resampler state
        m_ring_write_head = 0;
        m_ring_read_tail = 0;
        m_ring_count = 0;
        m_decode_avail_48k = 0;
        m_resample_src_pos = 0.0;

        // Start playback thread with high priority (0x12)
        m_thread_running = true;
        AudioPlayer* self = this;
        m_thread_id = sceKernelCreateThread("audio_thread", audioThreadFunc, 0x12, 0x10000, 0, nullptr);
        if (m_thread_id >= 0) {
            sceKernelStartThread(m_thread_id, sizeof(void*), &self);
        } else {
            m_use_fallback_clock = true;
        }
    }
#endif

    // Initialize fallback clock
#ifdef __PSP__
    sceRtcGetCurrentTick(&m_fallback_start_tick);
#endif
    m_fallback_time_offset = 0.0;
    m_playing = false;

    return m_loaded;
}

void AudioPlayer::play() {
    if (m_playing) return;
    m_playing = true;

#ifdef __PSP__
    sceRtcGetCurrentTick(&m_fallback_start_tick);
#endif
}

void AudioPlayer::pause() {
    if (!m_playing) return;
    m_playing = false;

#ifdef __PSP__
    uint64_t now;
    sceRtcGetCurrentTick(&now);
    uint32_t freq = sceRtcGetTickResolution();
    double elapsed = static_cast<double>(now - m_fallback_start_tick) / freq;
    m_fallback_time_offset += elapsed;
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

    m_fallback_time_offset = seconds;
#ifdef __PSP__
    sceRtcGetCurrentTick(&m_fallback_start_tick);
#endif
}

void AudioPlayer::setVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > PSP_AUDIO_VOLUME_MAX) volume = PSP_AUDIO_VOLUME_MAX;
    m_volume = volume;
}

double AudioPlayer::getTimeSeconds() const {
    if (m_loaded && !m_use_fallback_clock) {
        return static_cast<double>(m_current_samples) / NATIVE_SAMPLE_RATE;
    }

#ifdef __PSP__
    if (!m_playing) {
        return m_fallback_time_offset;
    }
    uint64_t now;
    sceRtcGetCurrentTick(&now);
    uint32_t freq = sceRtcGetTickResolution();
    double elapsed = static_cast<double>(now - m_fallback_start_tick) / freq;
    return m_fallback_time_offset + elapsed;
#else
    return m_fallback_time_offset;
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
