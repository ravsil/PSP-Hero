#include "highway_renderer.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

// Prototype projection constants
static const float YT = 12.0f;
static const float YH = 222.0f;
static const float SCR_H = 272.0f;
static const float CX = 240.0f;
static const float LW = 33.0f;
static const float K = 2.6f;
static const float SMIN = 1.0f / (1.0f + K);
static const float SB = SMIN + (SCR_H - YT) / (YH - YT) * (1.0f - SMIN);

HighwayRenderer::HighwayRenderer() {
}

HighwayRenderer::~HighwayRenderer() {
}

float HighwayRenderer::getScale(float d) const {
    return (d >= 0.0f) ? (1.0f / (1.0f + K * d)) : (1.0f - K * d);
}

float HighwayRenderer::getY(float scale) const {
    return YT + (YH - YT) * (scale - SMIN) / (1.0f - SMIN);
}

float HighwayRenderer::getLaneX(int lane, float scale) const {
    return CX + (lane - 2) * LW * scale;
}

float HighwayRenderer::getHighwayHalfWidth(float scale) const {
    return (2.5f * LW + 5.0f) * scale;
}

void HighwayRenderer::triggerHitFX(int lane, int streak) {
    if (lane < 0 || lane >= 5) return;

    HitRing ring;
    ring.lane = lane;
    ring.timer = 1.0f;
    m_rings.push_back(ring);

    // Particle sparks
    int count = 6 + std::min(10, streak / 6);
    float lx = getLaneX(lane, 1.0f);
    unsigned int col = Renderer::getLaneColor(lane);

    for (int i = 0; i < count; ++i) {
        SparkParticle p;
        p.x = lx;
        p.y = YH;
        p.vx = ((std::rand() % 100) / 50.0f - 1.0f) * 60.0f;
        p.vy = -30.0f - ((std::rand() % 100) / 100.0f) * 80.0f;
        p.life = 1.0f;
        p.color = col;
        m_sparks.push_back(p);
    }
}

void HighwayRenderer::update(float dt) {
    // Rings
    for (size_t i = 0; i < m_rings.size(); ) {
        m_rings[i].timer -= dt * 4.0f;
        if (m_rings[i].timer <= 0.0f) {
            m_rings.erase(m_rings.begin() + i);
        } else {
            ++i;
        }
    }

    // Sparks
    for (size_t i = 0; i < m_sparks.size(); ) {
        m_sparks[i].x += m_sparks[i].vx * dt;
        m_sparks[i].y += m_sparks[i].vy * dt;
        m_sparks[i].vy += 220.0f * dt; // gravity
        m_sparks[i].life -= dt * 2.5f;
        if (m_sparks[i].life <= 0.0f) {
            m_sparks.erase(m_sparks.begin() + i);
        } else {
            ++i;
        }
    }
}

void HighwayRenderer::renderHighwayBase(double current_time, double travel_time, uint32_t held_frets_mask, bool star_active) {
    (void)current_time;
    (void)travel_time;

    float hw_top = getHighwayHalfWidth(SMIN);
    float hw_bot = getHighwayHalfWidth(SB);

    // Highway bed
    unsigned int bed_col = COLOR_RGBA(9, 8, 16, 215);
    g_renderer.drawQuad(static_cast<int>(CX - hw_top), static_cast<int>(YT),
                        static_cast<int>(CX + hw_top), static_cast<int>(YT),
                        static_cast<int>(CX + hw_bot), static_cast<int>(SCR_H),
                        static_cast<int>(CX - hw_bot), static_cast<int>(SCR_H),
                        bed_col);

    // Pressed lane glow bands
    for (int i = 0; i < 5; ++i) {
        if (held_frets_mask & (1u << i)) {
            float x0_top = getLaneX(i, SMIN) - 0.5f * LW * SMIN;
            float x1_top = getLaneX(i, SMIN) + 0.5f * LW * SMIN;
            float x0_bot = getLaneX(i, SB) - 0.5f * LW * SB;
            float x1_bot = getLaneX(i, SB) + 0.5f * LW * SB;

            unsigned int col = (Renderer::getLaneColor(i) & 0x00FFFFFF) | 0x40000000;
            g_renderer.drawQuad(static_cast<int>(x0_top), static_cast<int>(YT + 20),
                                static_cast<int>(x1_top), static_cast<int>(YT + 20),
                                static_cast<int>(x1_bot), static_cast<int>(YH),
                                static_cast<int>(x0_bot), static_cast<int>(YH),
                                col);
        }
    }

    // Lane dividers
    for (int i = 1; i < 5; ++i) {
        float x_top = getLaneX(i, SMIN) - 0.5f * LW * SMIN;
        float x_bot = getLaneX(i, SB) - 0.5f * LW * SB;
        g_renderer.drawLine(static_cast<int>(x_top), static_cast<int>(YT),
                            static_cast<int>(x_bot), static_cast<int>(SCR_H),
                            COLOR_RGBA(255, 255, 255, 25));
    }

    // Side rails
    unsigned int rail_col = star_active ? COLOR_CYAN : COLOR_RGBA(170, 160, 210, 180);
    g_renderer.drawLine(static_cast<int>(CX - hw_top), static_cast<int>(YT),
                        static_cast<int>(CX - hw_bot), static_cast<int>(SCR_H),
                        rail_col);
    g_renderer.drawLine(static_cast<int>(CX + hw_top), static_cast<int>(YT),
                        static_cast<int>(CX + hw_bot), static_cast<int>(SCR_H),
                        rail_col);

    // Hit strikeline
    float hw_strike = getHighwayHalfWidth(1.0f);
    g_renderer.drawLine(static_cast<int>(CX - hw_strike), static_cast<int>(YH),
                        static_cast<int>(CX + hw_strike), static_cast<int>(YH),
                        COLOR_RGBA(255, 255, 255, 60));
}

void HighwayRenderer::renderNotes(const ChartTrack& track, double current_time, double travel_time) {
    if (travel_time <= 0.0) travel_time = 1.7;

    // 1. Sustain tails first
    for (const auto& note : track.notes) {
        if (note.duration_seconds <= 0.05) continue;

        double d_start = (note.time_seconds - current_time) / travel_time;
        double d_end = (note.time_seconds + note.duration_seconds - current_time) / travel_time;

        if (d_end < -0.1 || d_start > 1.05) continue;

        float ds = std::max(0.0f, static_cast<float>(d_start));
        float de = std::min(1.0f, static_cast<float>(d_end));
        if (de <= ds) continue;

        float s1 = getScale(ds);
        float s2 = getScale(de);
        float y1 = getY(s1);
        float y2 = getY(s2);

        if (note.lane >= 0 && note.lane < 5) {
            float x1 = getLaneX(note.lane, s1);
            float x2 = getLaneX(note.lane, s2);
            float w1 = 3.5f * s1;
            float w2 = 3.5f * s2;

            unsigned int col = Renderer::getLaneColor(note.lane);
            unsigned int tail_col = (col & 0x00FFFFFF) | 0x90000000;

            g_renderer.drawQuad(static_cast<int>(x1 - w1), static_cast<int>(y1),
                                static_cast<int>(x1 + w1), static_cast<int>(y1),
                                static_cast<int>(x2 + w2), static_cast<int>(y2),
                                static_cast<int>(x2 - w2), static_cast<int>(y2),
                                tail_col);

            // Center glow line
            g_renderer.drawLine(static_cast<int>(x1), static_cast<int>(y1),
                                static_cast<int>(x2), static_cast<int>(y2),
                                COLOR_RGBA(255, 255, 255, 180));
        }
    }

    // 2. Note gems (rendered from horizon to strikeline)
    for (auto it = track.notes.rbegin(); it != track.notes.rend(); ++it) {
        const auto& note = *it;
        if (note.hit) continue;

        double d = (note.time_seconds - current_time) / travel_time;
        if (d < -0.2 || d > 1.02) continue;

        float s = getScale(static_cast<float>(d));
        float y = getY(s);
        if (y < YT || y > SCR_H + 10) continue;

        if (note.lane == LANE_OPEN) {
            // Open strum purple bar across all lanes
            float hw = getHighwayHalfWidth(s) * 0.92f;
            int bar_h = std::max(3, static_cast<int>(6.0f * s));
            g_renderer.drawRoundedRect(static_cast<int>(CX - hw), static_cast<int>(y - bar_h / 2),
                                       static_cast<int>(hw * 2), bar_h, 2,
                                       COLOR_LANE_OPEN, COLOR_WHITE);
        } else if (note.lane >= 0 && note.lane < 5) {
            float x = getLaneX(note.lane, s);
            float rx = 13.0f * s;
            float ry = 8.6f * s;

            unsigned int col = note.missed ? COLOR_GRAY : Renderer::getLaneColor(note.lane);

            // Dark outer rim
            g_renderer.drawEllipse(static_cast<int>(x), static_cast<int>(y),
                                   static_cast<int>(rx + 2.0f * s), static_cast<int>(ry + 2.0f * s),
                                   COLOR_RGBA(6, 5, 10, 255), true);

            // Colored body
            g_renderer.drawEllipse(static_cast<int>(x), static_cast<int>(y),
                                   static_cast<int>(rx), static_cast<int>(ry),
                                   col, true);

            // 3D reflection highlight core
            if (note.is_hopo) {
                // HOPO: white center core
                g_renderer.drawEllipse(static_cast<int>(x), static_cast<int>(y),
                                       static_cast<int>(rx * 0.55f), static_cast<int>(ry * 0.55f),
                                       COLOR_WHITE, true);
            } else if (note.is_tap) {
                // Tap: translucent center
                g_renderer.drawEllipse(static_cast<int>(x), static_cast<int>(y),
                                       static_cast<int>(rx * 0.6f), static_cast<int>(ry * 0.6f),
                                       COLOR_RGBA(255, 255, 255, 180), true);
            } else {
                // Strum note: top shine specular reflection
                g_renderer.drawEllipse(static_cast<int>(x), static_cast<int>(y - 0.5f * s),
                                       static_cast<int>(rx * 0.5f), static_cast<int>(ry * 0.45f),
                                       COLOR_RGBA(255, 255, 255, 220), true);
            }
        }
    }
}

void HighwayRenderer::renderReceptors(uint32_t held_frets_mask) {
    for (int i = 0; i < 5; ++i) {
        float x = getLaneX(i, 1.0f);
        bool lit = (held_frets_mask & (1u << i)) != 0;
        unsigned int col = Renderer::getLaneColor(i);

        // Fret outer bezel
        g_renderer.drawEllipse(static_cast<int>(x), static_cast<int>(YH), 17, 12, COLOR_RGBA(5, 4, 8, 255), true);

        // Inner receptor body
        unsigned int body_col = lit ? col : ((col & 0x00FFFFFF) | 0x65000000);
        g_renderer.drawEllipse(static_cast<int>(x), static_cast<int>(YH), 14, 9, body_col, true);

        // Top specular highlight
        unsigned int hi_col = lit ? COLOR_WHITE : COLOR_RGBA(255, 255, 255, 80);
        g_renderer.drawEllipse(static_cast<int>(x), static_cast<int>(YH - 1), 7, 4, hi_col, true);
    }
}

void HighwayRenderer::renderEffects() {
    // Hit rings
    for (const auto& r : m_rings) {
        float x = getLaneX(r.lane, 1.0f);
        float progress = 1.0f - r.timer;
        int rx = static_cast<int>(15.0f + progress * 16.0f);
        int ry = static_cast<int>(10.0f + progress * 9.0f);
        unsigned int col = (Renderer::getLaneColor(r.lane) & 0x00FFFFFF) | static_cast<unsigned int>(r.timer * 200.0f) << 24;
        g_renderer.drawEllipse(static_cast<int>(x), static_cast<int>(YH), rx, ry, col, false);
    }

    // Sparks
    for (const auto& p : m_sparks) {
        int a = static_cast<int>(p.life * 255.0f);
        if (a > 255) a = 255;
        if (a < 0) a = 0;
        unsigned int c = (p.color & 0x00FFFFFF) | (a << 24);
        g_renderer.drawRect(static_cast<int>(p.x - 1), static_cast<int>(p.y - 1), 2, 2, c);
    }
}

void HighwayRenderer::renderHUD(const ChartSong& song, const ChartTrack& track, double current_time, const GameSession& session) {
    (void)track;

    // 1. Top progress bar across entire screen
    g_renderer.drawRect(0, 0, 480, 2, COLOR_RGBA(255, 255, 255, 20));
    if (song.total_duration_seconds > 0.0) {
        float prog = static_cast<float>(current_time / song.total_duration_seconds);
        if (prog > 1.0f) prog = 1.0f;
        if (prog < 0.0f) prog = 0.0f;
        unsigned int prog_col = session.star_active ? COLOR_CYAN : COLOR_RGBA(170, 200, 255, 240);
        g_renderer.drawRect(0, 0, static_cast<int>(480.0f * prog), 2, prog_col);
    }

    // 2. Song title & artist at top left
    g_renderer.drawText(10, 6, song.name, COLOR_WHITE, 1);
    g_renderer.drawText(10, 16, song.artist, COLOR_GRAY, 1);

    // 3. Score counter
    g_renderer.drawTextRight(110, 32, "Score", COLOR_GRAY, 1);
    char score_buf[32];
    std::snprintf(score_buf, sizeof(score_buf), "%u", session.score);
    g_renderer.drawTextRight(110, 44, score_buf, COLOR_WHITE, 2);

    // 4. Circular Multiplier Orb on left (ox = 58, oy = 124, r = 26)
    int ox = 58, oy = 124, oradius = 26;
    g_renderer.drawCircle(ox, oy, oradius, COLOR_PANEL_BG, true);
    g_renderer.drawCircle(ox, oy, oradius, COLOR_PANEL_BORDER, false);

    // Progress arc
    int step = session.streak % 10;
    float arc_end = -1.5707963f + (step / 10.0f) * 6.2831853f;
    if (session.multiplier >= 4) arc_end = 4.712389f;

    unsigned int mult_col = session.star_active ? COLOR_CYAN :
        (session.multiplier >= 4 ? COLOR_LANE_ORANGE :
        (session.multiplier == 3 ? COLOR_LANE_YELLOW :
        (session.multiplier == 2 ? COLOR_LANE_GREEN : COLOR_WHITE)));

    g_renderer.drawArc(ox, oy, oradius - 2, -1.5707963f, arc_end, mult_col, 3);

    char mult_buf[16];
    std::snprintf(mult_buf, sizeof(mult_buf), "x%u", session.multiplier);
    g_renderer.drawTextCentered(ox, oy - 4, mult_buf, mult_col, 2);

    // 5. Streak counter
    g_renderer.drawTextCentered(ox, 162, "Streak", COLOR_GRAY, 1);
    char streak_buf[16];
    std::snprintf(streak_buf, sizeof(streak_buf), "%u", session.streak);
    g_renderer.drawTextCentered(ox, 174, streak_buf, COLOR_WHITE, 1);

    // 6. Accuracy percentage
    char acc_buf[32];
    std::snprintf(acc_buf, sizeof(acc_buf), "%d%% accuracy", static_cast<int>(session.accuracy * 100.0));
    g_renderer.drawText(10, 258, acc_buf, COLOR_GRAY, 1);

    // 7. Rock Meter on right (mx = 438, my = 58, mh = 130, mw = 12)
    int mx = 438, my = 58, mh = 130, mw = 12;
    g_renderer.drawTextCentered(mx + mw / 2, 46, "Rock", COLOR_GRAY, 1);

    // Red (bottom 33%), Yellow (middle 33%), Green (top 33%)
    int third = mh / 3;
    g_renderer.drawRect(mx, my, mw, third, COLOR_RGBA(53, 211, 106, 70));
    g_renderer.drawRect(mx, my + third, mw, third, COLOR_RGBA(255, 210, 63, 70));
    g_renderer.drawRect(mx, my + 2 * third, mw, third, COLOR_RGBA(255, 75, 75, 70));

    // Active fill
    int fill_h = static_cast<int>(mh * session.rock);
    unsigned int active_col = (session.rock > 0.66f) ? COLOR_LANE_GREEN :
                              ((session.rock > 0.33f) ? COLOR_LANE_YELLOW : COLOR_LANE_RED);
    g_renderer.drawRect(mx, my + mh - fill_h, mw, fill_h, active_col);
    g_renderer.drawRectOutline(mx, my, mw, mh, COLOR_RGBA(255, 255, 255, 70));

    // Needle pointer
    int py = my + mh - fill_h;
    g_renderer.drawLine(mx - 6, py - 3, mx, py, COLOR_WHITE);
    g_renderer.drawLine(mx - 6, py + 3, mx, py, COLOR_WHITE);
    g_renderer.drawLine(mx - 6, py - 3, mx - 6, py + 3, COLOR_WHITE);

    // 8. Star Power Meter (sx = 460)
    int sx = 460;
    g_renderer.drawTextCentered(sx + mw / 2, 46, "Star", COLOR_GRAY, 1);
    g_renderer.drawRect(sx, my, mw, mh, COLOR_RGBA(61, 139, 255, 45));

    int star_h = static_cast<int>(mh * session.star);
    g_renderer.drawRect(sx, my + mh - star_h, mw, star_h, COLOR_CYAN);
    // 50% marker line
    g_renderer.drawLine(sx, my + mh / 2, sx + mw, my + mh / 2, COLOR_RGBA(255, 255, 255, 120));
    g_renderer.drawRectOutline(sx, my, mw, mh, COLOR_RGBA(255, 255, 255, 70));

    if (session.star_active) {
        g_renderer.drawTextCentered(sx + mw / 2, my + mh + 4, "Active", COLOR_CYAN, 1);
    } else if (session.star >= 0.5f) {
        g_renderer.drawTextCentered(sx + mw / 2, my + mh + 4, "Ready", COLOR_WHITE, 1);
    }

    // 9. Popups
    if (session.pop.timer > 0.0f) {
        g_renderer.drawTextCentered(static_cast<int>(CX), 170, session.pop.text, session.pop.color, 2);
    }
    if (session.mile.timer > 0.0f) {
        g_renderer.drawTextCentered(static_cast<int>(CX), 110, session.mile.text, COLOR_WHITE, 1);
    }
}

void HighwayRenderer::render(const ChartSong& song,
                             const ChartTrack& track,
                             double current_time,
                             double travel_time,
                             const GameSession& session,
                             uint32_t held_frets_mask,
                             bool is_paused) {
    (void)is_paused;

    // 1. Reactive Background
    g_renderer.renderBackground(session.star_active);

    // 2. Highway Base & Rails
    renderHighwayBase(current_time, travel_time, held_frets_mask, session.star_active);

    // 3. Notes & Sustains
    renderNotes(track, current_time, travel_time);

    // 4. Receptors at strikeline
    renderReceptors(held_frets_mask);

    // 5. Hit Rings & Sparks
    renderEffects();

    // 6. In-Game HUD
    renderHUD(song, track, current_time, session);
}
