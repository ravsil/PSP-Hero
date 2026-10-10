#include "gameplay.h"
#include "renderer.h"
#include <cmath>
#include <algorithm>

GameSession::GameSession() {
    reset();
}

void GameSession::reset() {
    score = 0;
    streak = 0;
    max_streak = 0;
    multiplier = 1;
    notes_hit = 0;
    notes_missed = 0;
    accuracy = 1.0;

    rock = 0.5f;
    star = 0.0f;
    star_active = false;
    star_timer = 0.0f;

    pop.text = "";
    pop.color = COLOR_WHITE;
    pop.timer = 0.0f;

    mile.text = "";
    mile.timer = 0.0f;

    failed = false;
    completed = false;
    is_paused = false;
    sustain_score_accumulator = 0.0;
}

void GameSession::addSustainScore(double seconds) {
    if (seconds <= 0.0) return;
    sustain_score_accumulator += seconds * 100.0 * multiplier;
    if (sustain_score_accumulator >= 1.0) {
        uint32_t whole = static_cast<uint32_t>(sustain_score_accumulator);
        score += whole;
        sustain_score_accumulator -= whole;
    }
}

void GameSession::onHit(int lane, double time_diff, int note_count) {
    (void)lane;
    notes_hit += note_count;
    streak += note_count;
    if (streak > max_streak) max_streak = streak;

    // Multiplier calculation: 10 notes per step, max 4x (doubled to 8x if star power active)
    uint32_t base_mult = std::min<uint32_t>(4, 1 + (streak / 10));
    multiplier = base_mult * (star_active ? 2 : 1);

    score += 50 * note_count * multiplier;

    // Rock meter boost
    double abs_diff = std::abs(time_diff);
    float rock_boost = (abs_diff < 0.04) ? 0.024f : 0.018f;
    if (star_active) rock_boost += 0.01f;
    rock = std::min(1.0f, rock + rock_boost);

    // Star power charge
    if (!star_active) {
        star = std::min(1.0f, star + 0.022f);
    }

    // Judgement popup
    if (abs_diff < 0.04) {
        pop.text = "Perfect";
        pop.color = COLOR_GOLD;
    } else if (abs_diff < 0.08) {
        pop.text = "Good";
        pop.color = COLOR_RGBA(216, 245, 224, 255);
    } else if (time_diff < 0) {
        pop.text = "Early";
        pop.color = COLOR_RGBA(170, 176, 196, 255);
    } else {
        pop.text = "Late";
        pop.color = COLOR_RGBA(170, 176, 196, 255);
    }
    pop.timer = 0.7f;

    // Milestone streak popups
    if (streak > 0 && streak % 25 == 0) {
        mile.text = std::to_string(streak) + " note streak";
        mile.timer = 1.2f;
    } else if (streak == 10 || streak == 20 || streak == 30) {
        mile.text = "Multiplier x" + std::to_string(multiplier);
        mile.timer = 1.0f;
    }

    uint32_t total = notes_hit + notes_missed;
    accuracy = (total > 0) ? (static_cast<double>(notes_hit) / total) : 1.0;
}

void GameSession::onMiss() {
    notes_missed++;
    streak = 0;
    multiplier = (star_active ? 2 : 1);

    rock = std::max(0.0f, rock - 0.06f);
    if (rock <= 0.0f) {
        failed = true;
    }

    pop.text = "Miss";
    pop.color = COLOR_RGBA(255, 107, 107, 255);
    pop.timer = 0.6f;

    uint32_t total = notes_hit + notes_missed;
    accuracy = (total > 0) ? (static_cast<double>(notes_hit) / total) : 1.0;
}

void GameSession::onOverstrum() {
    streak = 0;
    multiplier = (star_active ? 2 : 1);
    rock = std::max(0.0f, rock - 0.035f);
    if (rock <= 0.0f) {
        failed = true;
    }

    pop.text = "Miss";
    pop.color = COLOR_RGBA(255, 107, 107, 255);
    pop.timer = 0.5f;
}

void GameSession::activateStarPower() {
    if (star >= 0.5f && !star_active) {
        star_active = true;
        star_timer = 10.0f; // 10 seconds of star power
        multiplier = std::min<uint32_t>(4, 1 + (streak / 10)) * 2;
    }
}

void GameSession::update(float dt) {
    if (pop.timer > 0.0f) pop.timer -= dt;
    if (mile.timer > 0.0f) mile.timer -= dt;

    if (star_active) {
        star_timer -= dt;
        star = std::max(0.0f, star_timer / 10.0f);
        if (star_timer <= 0.0f) {
            star_active = false;
            star = 0.0f;
            multiplier = std::min<uint32_t>(4, 1 + (streak / 10));
        }
    }
}

int GameSession::calculateStars() const {
    if (failed) return 1;
    if (accuracy >= 0.95) return 5;
    if (accuracy >= 0.85) return 4;
    if (accuracy >= 0.70) return 3;
    if (accuracy >= 0.50) return 2;
    return 1;
}
