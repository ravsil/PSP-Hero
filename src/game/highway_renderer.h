#ifndef HIGHWAY_RENDERER_H
#define HIGHWAY_RENDERER_H

#include "chart_parser.h"
#include <string>

// RGBA to PSP GU ABGR helper
#define COLOR_RGBA(r, g, b, a) ((unsigned int)(((static_cast<unsigned int>(a) & 0xFF) << 24) | \
                                               ((static_cast<unsigned int>(b) & 0xFF) << 16) | \
                                               ((static_cast<unsigned int>(g) & 0xFF) << 8)  | \
                                               (static_cast<unsigned int>(r) & 0xFF)))

#define COLOR_WHITE   COLOR_RGBA(255, 255, 255, 255)
#define COLOR_BLACK   COLOR_RGBA(0, 0, 0, 255)
#define COLOR_GRAY    COLOR_RGBA(120, 120, 120, 255)
#define COLOR_DARK    COLOR_RGBA(30, 30, 35, 255)
#define COLOR_CYAN    COLOR_RGBA(0, 220, 255, 255)
#define COLOR_GOLD    COLOR_RGBA(255, 215, 0, 255)

// Fret colors (standard Guitar Hero / Clone Hero palette)
#define COLOR_FRET_GREEN   COLOR_RGBA(40, 220, 40, 255)
#define COLOR_FRET_RED     COLOR_RGBA(235, 45, 45, 255)
#define COLOR_FRET_YELLOW  COLOR_RGBA(245, 230, 45, 255)
#define COLOR_FRET_BLUE    COLOR_RGBA(45, 120, 245, 255)
#define COLOR_FRET_ORANGE  COLOR_RGBA(255, 135, 30, 255)
#define COLOR_FRET_OPEN    COLOR_RGBA(180, 50, 240, 255) // Purple bar

// Hit spark effect
struct HitSpark {
    float x;
    float y;
    float timer;
    unsigned int color;
};

// Gameplay statistics
struct GameplayStats {
    uint32_t score = 0;
    uint32_t streak = 0;
    uint32_t max_streak = 0;
    uint32_t multiplier = 1;
    uint32_t notes_hit = 0;
    uint32_t notes_missed = 0;
    bool autoplay = true;
};

class HighwayRenderer {
public:
    HighwayRenderer();
    ~HighwayRenderer();

    // Initialize GU graphics, textures, and font
    bool init();

    // Begin new frame rendering
    void beginFrame();

    // Render the entire highway, notes, strikeline, and HUD
    void render(const ChartSong& song,
                const ChartTrack& track,
                double current_time,
                const GameplayStats& stats,
                bool is_paused,
                uint32_t held_frets_mask);

    // End frame rendering and swap buffers
    void endFrame();

    // Trigger hit spark animation on a fret
    void triggerHitSpark(int lane);

    // Shutdown and free graphics resources
    void shutdown();

    // 2D drawing primitives
    void drawRect(int x, int y, int w, int h, unsigned int color);
    void drawRectOutline(int x, int y, int w, int h, unsigned int color);
    void drawQuad(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, unsigned int color);
    void drawLine(int x1, int y1, int x2, int y2, unsigned int color);
    void drawText(int x, int y, const std::string& text, unsigned int color, int scale = 1);

private:
    void* m_draw_buffer;
    void* m_disp_buffer;
    void* m_depth_buffer;

    // Font texture in VRAM/RAM
    void* m_font_texture;

    // Hit sparks
    static const int MAX_SPARKS = 8;
    HitSpark m_sparks[MAX_SPARKS];

    void initFontTexture();
    void updateSparks(float dt);
    void renderHighway(double current_time);
    void renderReceptors(uint32_t held_frets_mask);
    void renderNotes(const ChartTrack& track, double current_time);
    void renderHUD(const ChartSong& song, const ChartTrack& track, double current_time, const GameplayStats& stats, bool is_paused);

    // Geometry helpers
    float getHighwayWidthAt(float y) const;
    float getLaneCenterXAt(int lane, float y) const;
    float timeToY(double note_time, double current_time) const;
};

#endif // HIGHWAY_RENDERER_H

