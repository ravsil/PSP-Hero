#ifndef RENDERER_H
#define RENDERER_H

#include <string>
#include <vector>
#include <cstdint>

// RGBA to PSP GU ABGR helper
#define COLOR_RGBA(r, g, b, a) ((unsigned int)(((static_cast<unsigned int>(a) & 0xFF) << 24) | \
                                                ((static_cast<unsigned int>(b) & 0xFF) << 16) | \
                                                ((static_cast<unsigned int>(g) & 0xFF) << 8)  | \
                                                (static_cast<unsigned int>(r) & 0xFF)))

// Prototype-accurate color palette
#define COLOR_PAGE        COLOR_RGBA(7, 6, 12, 255)
#define COLOR_WHITE       COLOR_RGBA(255, 255, 255, 255)
#define COLOR_BLACK       COLOR_RGBA(0, 0, 0, 255)
#define COLOR_GRAY        COLOR_RGBA(140, 135, 154, 255)
#define COLOR_DIM         COLOR_RGBA(106, 101, 121, 255)
#define COLOR_PANEL_BG    COLOR_RGBA(8, 7, 14, 215)
#define COLOR_PANEL_BORDER COLOR_RGBA(255, 255, 255, 35)
#define COLOR_GOLD        COLOR_RGBA(255, 210, 63, 255)
#define COLOR_CYAN        COLOR_RGBA(170, 225, 255, 255)

// 5 Prototype lane colors: Green, Red, Yellow, Blue, Orange
#define COLOR_LANE_GREEN  COLOR_RGBA(53, 211, 106, 255)
#define COLOR_LANE_RED    COLOR_RGBA(255, 75, 75, 255)
#define COLOR_LANE_YELLOW COLOR_RGBA(255, 210, 63, 255)
#define COLOR_LANE_BLUE   COLOR_RGBA(61, 139, 255, 255)
#define COLOR_LANE_ORANGE COLOR_RGBA(255, 154, 60, 255)
#define COLOR_LANE_OPEN   COLOR_RGBA(190, 70, 255, 255)

enum ButtonIcon {
    ICON_CROSS = 0,
    ICON_CIRCLE,
    ICON_TRIANGLE,
    ICON_SQUARE,
    ICON_DPAD,
    ICON_DPAD_LR,
    ICON_START,
    ICON_SELECT
};

struct Texture {
    int width;
    int height;
    int pitch;
    uint32_t* pixels;
    bool is_valid;
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init();
    void shutdown();

    void beginFrame();
    void endFrame();

    // 2D Primitives
    void drawRect(int x, int y, int w, int h, unsigned int color);
    void drawRectOutline(int x, int y, int w, int h, unsigned int color);
    void drawRoundedRect(int x, int y, int w, int h, int r, unsigned int fill_color, unsigned int border_color = 0);
    void drawQuad(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, unsigned int color);
    void drawLine(int x1, int y1, int x2, int y2, unsigned int color);
    void drawCircle(int cx, int cy, int r, unsigned int color, bool fill = true);
    void drawEllipse(int cx, int cy, int rx, int ry, unsigned int color, bool fill = true);
    void drawArc(int cx, int cy, int r, float start_angle, float end_angle, unsigned int color, int thickness = 2);

    // Shapes & Icons
    void drawPick(int x, int y, unsigned int color);
    void drawStar(int cx, int cy, int r, bool filled);
    void drawButtonIcon(int x, int y, ButtonIcon icon);
    int  drawButtonHint(int x, int y, ButtonIcon icon, const std::string& label);

    // Text routines
    void drawText(int x, int y, const std::string& text, unsigned int color, int scale = 1);
    void drawTextCentered(int cx, int y, const std::string& text, unsigned int color, int scale = 1);
    void drawTextRight(int rx, int y, const std::string& text, unsigned int color, int scale = 1);
    void drawTextSpaced(int cx, int y, const std::string& text, unsigned int color, int spacing, int scale = 1);
    int  getTextWidth(const std::string& text, int scale = 1, int spacing = 0);

    // Dynamic Reactive Background
    void updateBackground(float dt, float bpm = 120.0f, bool star_power = false);
    void triggerBackgroundHit(int lane, float strength = 1.0f);
    void renderBackground(bool star_power = false, bool in_game = false);

    // Logo
    void drawLogo(int cx, int cy, int scale = 2);

    // Textures / Album Cover thumbnails
    Texture loadTexture(const std::string& filepath, int target_w = 64, int target_h = 64);
    void drawTexture(const Texture& tex, int x, int y, int w, int h);
    void freeTexture(Texture& tex);

    // Color utilities
    static unsigned int getLaneColor(int lane);
    static unsigned int lerpColor(unsigned int c1, unsigned int c2, float t);

private:
    void* m_draw_buffer;
    void* m_disp_buffer;
    void* m_depth_buffer;
    void* m_font_texture;

    // Reactive Background State
    float m_bg_tint[3];
    float m_target_tint[3];
    float m_bars[40];
    float m_ring_rot;
    float m_pulse;
    float m_energy;
    float m_star_timer;

    void initFontTexture();
};

extern Renderer g_renderer;

#endif // RENDERER_H

