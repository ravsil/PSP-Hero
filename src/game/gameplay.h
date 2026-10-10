#ifndef GAMEPLAY_H
#define GAMEPLAY_H

#include "chart_parser.h"
#include "config.h"
#include <string>
#include <vector>
#include <cstdint>

enum JudgementType {
    JUDGE_NONE = 0,
    JUDGE_PERFECT,
    JUDGE_GOOD,
    JUDGE_EARLY,
    JUDGE_LATE,
    JUDGE_MISS
};

struct JudgementPopup {
    std::string text;
    unsigned int color;
    float timer;     // Remaining seconds to display
};

struct MilestonePopup {
    std::string text;
    float timer;
};

struct GameSession {
    // Scoring
    uint32_t score;
    uint32_t streak;
    uint32_t max_streak;
    uint32_t multiplier;
    uint32_t notes_hit;
    uint32_t notes_missed;
    double accuracy;

    // Rock Meter (0.0 = fail, 0.5 = normal start, 1.0 = max)
    float rock;

    // Star Power
    float star;             // 0.0 to 1.0 (requires >= 0.5 to activate)
    bool star_active;
    float star_timer;

    // Popups
    JudgementPopup pop;
    MilestonePopup mile;

    // Status
    bool failed;
    bool completed;
    bool is_paused;

    GameSession();
    void reset();

    void onHit(int lane, double time_diff, int streak_mult);
    void onMiss();
    void onOverstrum();
    void activateStarPower();
    void update(float dt);

    int calculateStars() const;
};

#endif // GAMEPLAY_H
