#ifndef HIGHWAY_RENDERER_H
#define HIGHWAY_RENDERER_H

#include "chart_parser.h"
#include "gameplay.h"
#include "renderer.h"

#include <string>
#include <vector>

struct HitRing {
    int lane;
    float timer;
};

struct SparkParticle {
    float x;
    float y;
    float vx;
    float vy;
    float life;
    unsigned int color;
};

class HighwayRenderer {
public:
    HighwayRenderer();
    ~HighwayRenderer();

    // Trigger hit visual effects (rings, sparks)
    void triggerHitFX(int lane, int streak);
    void triggerSustainFX(int lane, bool star_active);

    // Update particles and animations
    void update(float dt);

    // Main render pass
    void render(const ChartSong& song,
                const ChartTrack& track,
                double current_time,
                double travel_time,
                const GameSession& session,
                uint32_t held_frets_mask,
                bool is_paused);

private:
    std::vector<HitRing> m_rings;
    std::vector<SparkParticle> m_sparks;

    // Projection helpers
    float getScale(float d) const;
    float getY(float scale) const;
    float getLaneX(int lane, float scale) const;
    float getHighwayHalfWidth(float scale) const;

    void renderHighwayBase(double current_time, double travel_time, uint32_t held_frets_mask, bool star_active);
    void renderNotes(const ChartTrack& track, double current_time, double travel_time, bool star_active);
    void renderReceptors(uint32_t held_frets_mask);
    void renderEffects();
    void renderHUD(const ChartSong& song, const ChartTrack& track, double current_time, const GameSession& session);
};

#endif // HIGHWAY_RENDERER_H
