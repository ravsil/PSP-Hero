#ifndef SOUND_EFFECTS_H
#define SOUND_EFFECTS_H

#include <cstdint>

enum SoundEffectId {
    SFX_TICK = 0,
    SFX_OPT,
    SFX_SELECT,
    SFX_BACK,
    SFX_STAR_READY,
    SFX_STAR_ON,
    SFX_MISS,
    SFX_COUNT
};

class SoundEffects {
public:
    static bool init();
    static void shutdown();
    static void play(SoundEffectId id);
    static void setVolume(int volume);
    static void setEnabled(bool enabled);
    static bool isEnabled();

private:
#ifdef __PSP__
    static int sfxThreadFunc(unsigned int args, void* argp);
#endif
};

#endif // SOUND_EFFECTS_H

