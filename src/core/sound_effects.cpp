#include "sound_effects.h"
#include "sound_effects_data.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>

#ifdef __PSP__
#include <pspkernel.h>
#include <pspaudio.h>
#endif

namespace {

constexpr int SFX_BLOCK_SAMPLES = 1024;
constexpr int MAX_VOICES = 4;
constexpr int QUEUE_SIZE = 16;

struct SfxDefinition {
    const int16_t* samples = nullptr;
    int sample_count = 0;
};

struct SfxVoice {
    SoundEffectId id;
    int offset = 0;
    bool active = false;
};

static SfxDefinition s_defs[SFX_COUNT];
static SfxVoice s_voices[MAX_VOICES];

static volatile SoundEffectId s_queue[QUEUE_SIZE];
static volatile int s_queue_write = 0;
static volatile int s_queue_read = 0;

static volatile bool s_thread_running = false;
static volatile int s_volume = 0x8000;
static volatile bool s_enabled = true;
static int s_thread_id = -1;

} // namespace

#ifdef __PSP__
int SoundEffects::sfxThreadFunc(unsigned int args, void* argp) {
    (void)args; (void)argp;

    int channel = -1;
    int16_t out_block[SFX_BLOCK_SAMPLES * 2];
    int16_t silence_block[SFX_BLOCK_SAMPLES * 2];
    std::memset(silence_block, 0, sizeof(silence_block));

    while (s_thread_running) {
        // Dequeue play requests
        while (s_queue_read != s_queue_write) {
            SoundEffectId id = s_queue[s_queue_read % QUEUE_SIZE];
            s_queue_read++;

            if (id >= 0 && id < SFX_COUNT && s_defs[id].samples) {
                int free_slot = -1;
                for (int v = 0; v < MAX_VOICES; ++v) {
                    if (!s_voices[v].active) {
                        free_slot = v;
                        break;
                    }
                }
                if (free_slot < 0) {
                    free_slot = 0; // steal voice 0
                }
                s_voices[free_slot].id = id;
                s_voices[free_slot].offset = 0;
                s_voices[free_slot].active = true;
            }
        }

        bool any_active = false;
        int32_t mix_l[SFX_BLOCK_SAMPLES] = {0};
        int32_t mix_r[SFX_BLOCK_SAMPLES] = {0};

        for (int v = 0; v < MAX_VOICES; ++v) {
            if (!s_voices[v].active) continue;

            const SfxDefinition& def = s_defs[s_voices[v].id];
            if (!def.samples || s_voices[v].offset >= def.sample_count) {
                s_voices[v].active = false;
                continue;
            }

            any_active = true;
            int remain = def.sample_count - s_voices[v].offset;
            int count = std::min(SFX_BLOCK_SAMPLES, remain);
            const int16_t* src = def.samples + s_voices[v].offset;

            for (int i = 0; i < count; ++i) {
                int16_t s = src[i];
                mix_l[i] += s;
                mix_r[i] += s;
            }

            s_voices[v].offset += count;
            if (s_voices[v].offset >= def.sample_count) {
                s_voices[v].active = false;
            }
        }

        if (channel < 0) {
            channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, SFX_BLOCK_SAMPLES, PSP_AUDIO_FORMAT_STEREO);
        }

        if (any_active) {
            if (channel >= 0) {
                for (int i = 0; i < SFX_BLOCK_SAMPLES; ++i) {
                    int32_t l = mix_l[i];
                    int32_t r = mix_r[i];
                    if (l > 32767) l = 32767; else if (l < -32768) l = -32768;
                    if (r > 32767) r = 32767; else if (r < -32768) r = -32768;
                    out_block[i * 2]     = static_cast<int16_t>(l);
                    out_block[i * 2 + 1] = static_cast<int16_t>(r);
                }
                sceAudioOutputPannedBlocking(channel, s_volume, s_volume, out_block);
            }
        } else {
            // Idle: sleep 5ms to yield CPU without thrashing channel reserve/release
            sceKernelDelayThread(5000);
        }
    }

    if (channel >= 0) {
        sceAudioChRelease(channel);
        channel = -1;
    }

    return 0;
}
#endif

bool SoundEffects::init() {
    // Instant zero-cost initialization from static tables
    s_defs[SFX_TICK].samples = s_pcm_tick;
    s_defs[SFX_TICK].sample_count = sizeof(s_pcm_tick) / sizeof(int16_t);

    s_defs[SFX_OPT].samples = s_pcm_opt;
    s_defs[SFX_OPT].sample_count = sizeof(s_pcm_opt) / sizeof(int16_t);

    s_defs[SFX_SELECT].samples = s_pcm_select;
    s_defs[SFX_SELECT].sample_count = sizeof(s_pcm_select) / sizeof(int16_t);

    s_defs[SFX_BACK].samples = s_pcm_back;
    s_defs[SFX_BACK].sample_count = sizeof(s_pcm_back) / sizeof(int16_t);

    s_defs[SFX_STAR_READY].samples = s_pcm_star_ready;
    s_defs[SFX_STAR_READY].sample_count = sizeof(s_pcm_star_ready) / sizeof(int16_t);

    s_defs[SFX_STAR_ON].samples = s_pcm_star_on;
    s_defs[SFX_STAR_ON].sample_count = sizeof(s_pcm_star_on) / sizeof(int16_t);

    s_defs[SFX_MISS].samples = s_pcm_miss;
    s_defs[SFX_MISS].sample_count = sizeof(s_pcm_miss) / sizeof(int16_t);

    s_queue_write = 0;
    s_queue_read = 0;
    for (int v = 0; v < MAX_VOICES; ++v) {
        s_voices[v].active = false;
        s_voices[v].offset = 0;
    }

    s_thread_running = true;

#ifdef __PSP__
    // Priority 0x22 (lower priority than main thread 0x20 so it never starves main loop)
    s_thread_id = sceKernelCreateThread("sfx_thread", sfxThreadFunc, 0x22, 0x8000, 0, nullptr);
    if (s_thread_id >= 0) {
        sceKernelStartThread(s_thread_id, 0, nullptr);
    }
#endif

    return true;
}

void SoundEffects::shutdown() {
    s_thread_running = false;

#ifdef __PSP__
    if (s_thread_id >= 0) {
        sceKernelWaitThreadEnd(s_thread_id, nullptr);
        sceKernelDeleteThread(s_thread_id);
        s_thread_id = -1;
    }
#endif
}

void SoundEffects::play(SoundEffectId id) {
    if (!s_enabled || id < 0 || id >= SFX_COUNT) return;

    int next_write = (s_queue_write + 1) % QUEUE_SIZE;
    if (next_write != (s_queue_read % QUEUE_SIZE)) {
        s_queue[s_queue_write % QUEUE_SIZE] = id;
        s_queue_write++;
    }
}

void SoundEffects::setVolume(int volume) {
    s_volume = std::clamp(volume, 0, 0x8000);
}

void SoundEffects::setEnabled(bool enabled) {
    s_enabled = enabled;
}

bool SoundEffects::isEnabled() {
    return s_enabled;
}
