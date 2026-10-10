#include "audio_player.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <vector>

#ifdef __PSP__
#include <pspkernel.h>
#include <pspaudio.h>
#include <psprtc.h>
#include <opus/opusfile.h>
#include <tremor/ivorbisfile.h>
#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"
#endif

AudioPlayer::AudioPlayer()
    : m_loaded(false)
    , m_playing(false)
    , m_thread_running(false)
    , m_seek_requested(false)
    , m_seek_target_seconds(0.0)
    , m_volume(PSP_AUDIO_VOLUME_MAX)
    , m_codec(AudioCodec::NONE)
    , m_opus_file(nullptr)
    , m_vorbis_file(nullptr)
    , m_mp3_dec(nullptr)
    , m_channels(2)
    , m_audio_channel(-1)
    , m_use_src_channel(false)
    , m_sample_rate(AUDIO_SAMPLE_RATE_44K)
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
    if (m_codec == AudioCodec::NONE || !m_ring_buffer) return;

    int space = RING_BUFFER_FRAMES - m_ring_count;
    if (space < 1024) return;

    int contiguous = RING_BUFFER_FRAMES - m_ring_write_head;
    int to_read = std::min(1024, std::min(space, contiguous));
    int frames_read = 0;

    if (m_codec == AudioCodec::OPUS) {
        OggOpusFile* of = reinterpret_cast<OggOpusFile*>(m_opus_file);
        if (of) {
            if (m_use_src_channel) {
                frames_read = op_read_stereo(of, m_ring_buffer + m_ring_write_head * 2, to_read * 2);
            } else {
                // 48kHz to 44.1kHz software fallback resampler
                const double ratio = 48000.0 / 44100.0;
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
                    int read_48k = op_read_stereo(of, m_decode_buf_48k + remain * 2, frames_to_read * 2);
                    if (read_48k > 0) {
                        m_decode_avail_48k = remain + read_48k;
                    }
                }
                while (static_cast<int>(m_resample_src_pos) + 1 < m_decode_avail_48k &&
                       frames_read < to_read) {
                    int idx = static_cast<int>(m_resample_src_pos);
                    float frac = static_cast<float>(m_resample_src_pos - idx);
                    int16_t l = static_cast<int16_t>((1.0f - frac) * m_decode_buf_48k[idx * 2] +
                                                    frac * m_decode_buf_48k[(idx + 1) * 2]);
                    int16_t r = static_cast<int16_t>((1.0f - frac) * m_decode_buf_48k[idx * 2 + 1] +
                                                    frac * m_decode_buf_48k[(idx + 1) * 2 + 1]);
                    m_ring_buffer[(m_ring_write_head + frames_read) * 2]     = l;
                    m_ring_buffer[(m_ring_write_head + frames_read) * 2 + 1] = r;
                    frames_read++;
                    m_resample_src_pos += ratio;
                }
            }
        }
    } else if (m_codec == AudioCodec::VORBIS) {
        OggVorbis_File* vf = reinterpret_cast<OggVorbis_File*>(m_vorbis_file);
        if (vf) {
            int total_frames = 0;
            if (m_channels == 2) {
                // Interleaved stereo: loop until all to_read frames are decoded
                while (total_frames < to_read) {
                    int bitstream = 0;
                    char* dst = reinterpret_cast<char*>(m_ring_buffer + (m_ring_write_head + total_frames) * 2);
                    int bytes_needed = (to_read - total_frames) * 4;
                    long bytes_read = ov_read(vf, dst, bytes_needed, &bitstream);
                    if (bytes_read > 0) {
                        total_frames += bytes_read / 4;
                    } else if (bytes_read == 0) {
                        break; // EOF
                    } else if (bytes_read == OV_HOLE) {
                        continue; // Bitstream hole, retry
                    } else {
                        break; // Fatal error
                    }
                }
            } else {
                // Mono: read mono samples, expand to stereo in ring buffer
                while (total_frames < to_read) {
                    int bitstream = 0;
                    int16_t mono_buf[1024];
                    int frames_to_read = std::min(1024, to_read - total_frames);
                    long bytes_read = ov_read(vf, reinterpret_cast<char*>(mono_buf), frames_to_read * 2, &bitstream);
                    if (bytes_read > 0) {
                        int read_samples = bytes_read / 2;
                        for (int i = 0; i < read_samples; ++i) {
                            int idx = (m_ring_write_head + total_frames + i) * 2;
                            m_ring_buffer[idx]     = mono_buf[i];
                            m_ring_buffer[idx + 1] = mono_buf[i];
                        }
                        total_frames += read_samples;
                    } else if (bytes_read == 0) {
                        break;
                    } else if (bytes_read == OV_HOLE) {
                        continue;
                    } else {
                        break;
                    }
                }
            }
            frames_read = total_frames;
        }
    } else if (m_codec == AudioCodec::MP3) {
        mp3dec_ex_t* mp3 = reinterpret_cast<mp3dec_ex_t*>(m_mp3_dec);
        if (mp3) {
            int total_frames = 0;
            if (m_channels == 2) {
                while (total_frames < to_read) {
                    int16_t* dst = m_ring_buffer + (m_ring_write_head + total_frames) * 2;
                    size_t samples_needed = (to_read - total_frames) * 2;
                    size_t samples_read = mp3dec_ex_read(mp3, dst, samples_needed);
                    if (samples_read > 0) {
                        total_frames += samples_read / 2;
                    } else {
                        break;
                    }
                }
            } else {
                while (total_frames < to_read) {
                    int16_t mono_buf[1024];
                    int frames_to_read = std::min(1024, to_read - total_frames);
                    size_t samples_read = mp3dec_ex_read(mp3, mono_buf, frames_to_read);
                    if (samples_read > 0) {
                        for (size_t i = 0; i < samples_read; ++i) {
                            int idx = (m_ring_write_head + total_frames + i) * 2;
                            m_ring_buffer[idx]     = mono_buf[i];
                            m_ring_buffer[idx + 1] = mono_buf[i];
                        }
                        total_frames += samples_read;
                    } else {
                        break;
                    }
                }
            }
            frames_read = total_frames;
        }
    }

    if (frames_read > 0) {
        m_ring_write_head = (m_ring_write_head + frames_read) % RING_BUFFER_FRAMES;
        m_ring_count += frames_read;
    }
#endif
}

void AudioPlayer::runAudioLoop() {
#ifdef __PSP__
    m_use_src_channel = false;
    m_audio_channel = -1;

    // Use hardware SRC channel ONLY for 48kHz audio (Opus / 48kHz streams)
    // For native 44.1kHz audio (standard CD audio & Vorbis), reserve the standard hardware channel directly.
    if (m_sample_rate == AUDIO_SAMPLE_RATE_48K) {
        int src_res = sceAudioSRCChReserve(AUDIO_BLOCK_SAMPLES, AUDIO_SAMPLE_RATE_48K, 2);
        if (src_res >= 0) {
            m_use_src_channel = true;
            m_audio_channel = src_res;
        }
    }

    if (!m_use_src_channel) {
        m_audio_channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, AUDIO_BLOCK_SAMPLES, PSP_AUDIO_FORMAT_STEREO);
        if (m_audio_channel < 0) {
            m_use_fallback_clock = true;
            return;
        }
    }

    int16_t out_block[AUDIO_BLOCK_SAMPLES * 2];
    int16_t silence_block[AUDIO_BLOCK_SAMPLES * 2];
    std::memset(silence_block, 0, sizeof(silence_block));

    // Initial pre-buffer (~8,192 frames, roughly 170-185ms)
    for (int i = 0; i < 8 && m_thread_running; ++i) {
        decodeChunk();
    }

    while (m_thread_running) {
        if (m_seek_requested) {
            int64_t target_samples = static_cast<int64_t>(m_seek_target_seconds * m_sample_rate);
            if (target_samples < 0) target_samples = 0;
            if (target_samples > m_total_samples) target_samples = m_total_samples;

            if (m_codec == AudioCodec::OPUS && m_opus_file) {
                op_pcm_seek(reinterpret_cast<OggOpusFile*>(m_opus_file), target_samples);
            } else if (m_codec == AudioCodec::VORBIS && m_vorbis_file) {
                ov_pcm_seek(reinterpret_cast<OggVorbis_File*>(m_vorbis_file), target_samples);
            } else if (m_codec == AudioCodec::MP3 && m_mp3_dec) {
                mp3dec_ex_seek(reinterpret_cast<mp3dec_ex_t*>(m_mp3_dec), target_samples * m_channels);
            }

            // Flush ring buffer
            m_ring_write_head = 0;
            m_ring_read_tail = 0;
            m_ring_count = 0;
            m_current_samples = target_samples;
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
    m_sample_rate = AUDIO_SAMPLE_RATE_44K;
    m_codec = AudioCodec::NONE;
    m_channels = 2;

#ifdef __PSP__
    sceRtcGetCurrentTick(&m_playback_start_tick);
    sceRtcGetCurrentTick(&m_last_block_tick);
#endif

    if (filepath.empty()) {
        return false;
    }

#ifdef __PSP__
    std::vector<std::string> path_candidates = {
        filepath,
        "songs/" + filepath,
        "song/" + filepath,
        "ms0:/PSP/GAME/cloneHero/" + filepath
    };

    std::string valid_path;
    for (const auto& p : path_candidates) {
        FILE* f = std::fopen(p.c_str(), "rb");
        if (f) {
            std::fclose(f);
            valid_path = p;
            break;
        }
    }
    if (valid_path.empty()) {
        valid_path = filepath;
    }

    // Determine extension in lowercase
    std::string ext;
    size_t dot = valid_path.find_last_of('.');
    if (dot != std::string::npos) {
        ext = valid_path.substr(dot);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    if (ext == ".opus") {
        int error = 0;
        OggOpusFile* of = op_open_file(valid_path.c_str(), &error);
        if (of) {
            op_set_dither_enabled(of, 0);
            m_opus_file = of;
            m_codec = AudioCodec::OPUS;
            m_sample_rate = AUDIO_SAMPLE_RATE_48K;
            m_channels = 2;
            m_total_samples = op_pcm_total(of, -1);
            m_total_duration = static_cast<double>(m_total_samples) / m_sample_rate;
            m_loaded = true;
        }
    } else if (ext == ".ogg") {
        // Try Tremor Vorbis first
        OggVorbis_File* vf = new OggVorbis_File();
        int ret = ov_fopen(valid_path.c_str(), vf);
        if (ret != 0) {
            FILE* f = std::fopen(valid_path.c_str(), "rb");
            if (f) {
                ret = ov_open(f, vf, NULL, 0);
                if (ret != 0) {
                    std::fclose(f);
                }
            } else {
                ret = -1;
            }
        }
        if (ret == 0) {
            vorbis_info* vi = ov_info(vf, -1);
            m_vorbis_file = vf;
            m_codec = AudioCodec::VORBIS;
            m_sample_rate = vi ? vi->rate : 44100;
            m_channels = vi ? vi->channels : 2;
            m_total_samples = ov_pcm_total(vf, -1);
            m_total_duration = static_cast<double>(m_total_samples) / m_sample_rate;
            m_loaded = true;
        } else {
            delete vf;
            // If it wasn't Vorbis, it might be an Opus stream with .ogg extension
            int error = 0;
            OggOpusFile* of = op_open_file(valid_path.c_str(), &error);
            if (of) {
                op_set_dither_enabled(of, 0);
                m_opus_file = of;
                m_codec = AudioCodec::OPUS;
                m_sample_rate = AUDIO_SAMPLE_RATE_48K;
                m_channels = 2;
                m_total_samples = op_pcm_total(of, -1);
                m_total_duration = static_cast<double>(m_total_samples) / m_sample_rate;
                m_loaded = true;
            }
        }
    } else if (ext == ".mp3") {
        mp3dec_ex_t* mp3 = new mp3dec_ex_t();
        if (mp3dec_ex_open(mp3, valid_path.c_str(), MP3D_SEEK_TO_SAMPLE) == 0) {
            m_mp3_dec = mp3;
            m_codec = AudioCodec::MP3;
            m_sample_rate = mp3->info.hz ? mp3->info.hz : 44100;
            m_channels = mp3->info.channels ? mp3->info.channels : 2;
            m_total_samples = mp3->samples / (m_channels > 0 ? m_channels : 1);
            m_total_duration = static_cast<double>(m_total_samples) / m_sample_rate;
            m_loaded = true;
        } else {
            delete mp3;
        }
    }

    if (m_loaded) {
        m_current_samples = 0;
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

    if (m_codec == AudioCodec::OPUS && m_opus_file) {
        op_free(reinterpret_cast<OggOpusFile*>(m_opus_file));
        m_opus_file = nullptr;
    } else if (m_codec == AudioCodec::VORBIS && m_vorbis_file) {
        OggVorbis_File* vf = reinterpret_cast<OggVorbis_File*>(m_vorbis_file);
        ov_clear(vf);
        delete vf;
        m_vorbis_file = nullptr;
    } else if (m_codec == AudioCodec::MP3 && m_mp3_dec) {
        mp3dec_ex_t* mp3 = reinterpret_cast<mp3dec_ex_t*>(m_mp3_dec);
        mp3dec_ex_close(mp3);
        delete mp3;
        m_mp3_dec = nullptr;
    }
#endif

    m_codec = AudioCodec::NONE;
    m_loaded = false;
}
