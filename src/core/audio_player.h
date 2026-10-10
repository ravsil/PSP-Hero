#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <string>
#include <cstdint>

#define NATIVE_SAMPLE_RATE 44100
#define OPUS_SAMPLE_RATE 48000
#define AUDIO_BLOCK_SAMPLES 1024 // Frames per hardware block (1024 * 2 values)
#define RING_BUFFER_FRAMES 32768 // 32768 stereo frames (~743 ms)

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();

    // Initialize audio system and load audio file (e.g. "song/guitar.opus")
    bool init(const std::string& filepath);

    // Audio playback controls
    void play();
    void pause();
    void togglePause();
    void seek(double seconds);
    void setVolume(int volume); // 0 to 0x8000 (32768)

    // Status queries
    bool isLoaded() const { return m_loaded; }
    bool isPlaying() const { return m_playing; }
    double getTimeSeconds() const;
    double getTotalDurationSeconds() const { return m_total_duration; }

    // Shutdown and release resources
    void shutdown();

private:
    bool m_loaded;
    volatile bool m_playing;
    volatile bool m_thread_running;
    volatile bool m_seek_requested;
    double m_seek_target_seconds;
    int m_volume;

    // Opus and PSP audio state
    void* m_opus_file; // OggOpusFile*
    int m_audio_channel;
    int m_thread_id;
    int64_t m_current_samples; // in 44.1kHz frames
    int64_t m_total_samples;   // in 48kHz frames
    double m_total_duration;

    // Ring buffer (holds 44.1kHz stereo audio frames)
    int16_t* m_ring_buffer; // size: RING_BUFFER_FRAMES * 2
    int m_ring_write_head;
    int m_ring_read_tail;
    volatile int m_ring_count;

    // Resampler scratch buffer (holds 48kHz decoded audio from Opus)
    int16_t m_decode_buf_48k[2048 * 2];
    int m_decode_avail_48k;
    double m_resample_src_pos;

    // Fallback clock (when audio file is missing or failed to open)
    uint64_t m_fallback_start_tick;
    uint64_t m_fallback_paused_tick;
    double m_fallback_time_offset;
    bool m_use_fallback_clock;

    static int audioThreadFunc(unsigned int args, void* argp);
    void runAudioLoop();
    void fillRingBuffer();
};

#endif // AUDIO_PLAYER_H
