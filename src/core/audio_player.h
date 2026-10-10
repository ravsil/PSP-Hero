#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <string>
#include <cstdint>

#define AUDIO_SAMPLE_RATE_48K 48000
#define AUDIO_SAMPLE_RATE_44K 44100
#define AUDIO_BLOCK_SAMPLES   1024
#define RING_BUFFER_FRAMES    16384 // ~341ms at 48kHz

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();

    // Initialize audio system and load audio file (e.g. "song/Aerials/guitar.opus")
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
    bool m_use_src_channel; // True if using hardware 48kHz SRC channel
    int m_sample_rate;
    int m_thread_id;
    int64_t m_current_samples;
    int64_t m_total_samples;
    double m_total_duration;

    // Ring buffer (holds stereo frames)
    int16_t* m_ring_buffer; // size: RING_BUFFER_FRAMES * 2
    int m_ring_write_head;
    int m_ring_read_tail;
    volatile int m_ring_count;

    // Resampler scratch buffer (only used if 44.1kHz fallback is active)
    int16_t m_decode_buf_48k[2048 * 2];
    int m_decode_avail_48k;
    double m_resample_src_pos;

    // Hardware sync clock for sub-block smooth interpolation
    uint64_t m_last_block_tick;
    uint64_t m_playback_start_tick;
    uint64_t m_paused_tick;
    double m_clock_offset;
    bool m_use_fallback_clock;

    static int audioThreadFunc(unsigned int args, void* argp);
    void runAudioLoop();
    void decodeChunk();
};

#endif // AUDIO_PLAYER_H
