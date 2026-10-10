#include "renderer.h"
#include "font8x8.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

#ifdef __PSP__
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#endif

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272
#define FRAME_SIZE (BUF_WIDTH * SCR_HEIGHT * 4)

#ifdef __PSP__
static unsigned int __attribute__((aligned(16))) dlist[262144];

struct VertexColor2D {
    unsigned int color;
    short x, y, z;
};

struct VertexTex2D {
    short u, v;
    unsigned int color;
    short x, y, z;
};
#endif

Renderer g_renderer;

Renderer::Renderer()
    : m_draw_buffer(nullptr)
    , m_disp_buffer(reinterpret_cast<void*>(FRAME_SIZE))
    , m_depth_buffer(reinterpret_cast<void*>(FRAME_SIZE * 2))
    , m_font_texture(nullptr)
    , m_ring_rot(0.0f)
    , m_pulse(0.0f)
    , m_energy(0.0f)
    , m_star_timer(0.0f)
{
    m_bg_tint[0] = 110.0f; m_bg_tint[1] = 80.0f; m_bg_tint[2] = 200.0f;
    m_target_tint[0] = 110.0f; m_target_tint[1] = 80.0f; m_target_tint[2] = 200.0f;
    for (int i = 0; i < 40; ++i) m_bars[i] = 0.05f;
}

Renderer::~Renderer() {
    shutdown();
}

void Renderer::initFontTexture() {
    size_t tex_bytes = 128 * 64 * sizeof(uint32_t);
    m_font_texture = std::malloc(tex_bytes);
    if (!m_font_texture) return;

    uint32_t* pixels = reinterpret_cast<uint32_t*>(m_font_texture);
    std::memset(pixels, 0, tex_bytes);

    for (int ch = 0; ch < 128; ++ch) {
        int col = ch % 16;
        int row = ch / 16;
        int bx = col * 8;
        int by = row * 8;

        for (int r = 0; r < 8; ++r) {
            unsigned char b = font8x8_basic[ch][r];
            for (int c = 0; c < 8; ++c) {
                bool on = (b & (1 << c)) != 0;
                pixels[(by + r) * 128 + (bx + c)] = on ? COLOR_WHITE : 0x00000000;
            }
        }
    }

#ifdef __PSP__
    sceKernelDcacheWritebackAll();
#endif
}

bool Renderer::init() {
    initFontTexture();

#ifdef __PSP__
    sceGuInit();
    sceGuStart(GU_DIRECT, dlist);
    sceGuDrawBuffer(GU_PSM_8888, m_draw_buffer, BUF_WIDTH);
    sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, m_disp_buffer, BUF_WIDTH);
    sceGuDepthBuffer(m_depth_buffer, BUF_WIDTH);
    sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuAlphaFunc(GU_GREATER, 0, 0xFF);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuFrontFace(GU_CW);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_CULL_FACE);
    sceGuEnable(GU_CLIP_PLANES);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
#endif

    return true;
}

void Renderer::shutdown() {
#ifdef __PSP__
    sceGuTerm();
#endif
    if (m_font_texture) {
        std::free(m_font_texture);
        m_font_texture = nullptr;
    }
}

void Renderer::beginFrame() {
#ifdef __PSP__
    sceGuStart(GU_DIRECT, dlist);
    sceGuClearColor(COLOR_PAGE);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
#endif
}

void Renderer::endFrame() {
#ifdef __PSP__
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
#endif
}

void Renderer::drawRect(int x, int y, int w, int h, unsigned int color) {
    drawQuad(x, y, x + w, y, x + w, y + h, x, y + h, color);
}

void Renderer::drawRectOutline(int x, int y, int w, int h, unsigned int color) {
    drawLine(x, y, x + w, y, color);
    drawLine(x + w, y, x + w, y + h, color);
    drawLine(x + w, y + h, x, y + h, color);
    drawLine(x, y + h, x, y, color);
}

void Renderer::drawRoundedRect(int x, int y, int w, int h, int r, unsigned int fill_color, unsigned int border_color) {
    if (r <= 0) {
        drawRect(x, y, w, h, fill_color);
        if (border_color != 0) drawRectOutline(x, y, w, h, border_color);
        return;
    }

    // Body
    drawRect(x + r, y, w - 2 * r, h, fill_color);
    drawRect(x, y + r, r, h - 2 * r, fill_color);
    drawRect(x + w - r, y + r, r, h - 2 * r, fill_color);

    // Corner fills
    drawCircle(x + r, y + r, r, fill_color, true);
    drawCircle(x + w - r, y + r, r, fill_color, true);
    drawCircle(x + r, y + h - r, r, fill_color, true);
    drawCircle(x + w - r, y + h - r, r, fill_color, true);

    if (border_color != 0) {
        drawLine(x + r, y, x + w - r, y, border_color);
        drawLine(x + r, y + h, x + w - r, y + h, border_color);
        drawLine(x, y + r, x, y + h - r, border_color);
        drawLine(x + w, y + r, x + w, y + h - r, border_color);
    }
}

void Renderer::drawQuad(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, unsigned int color) {
#ifdef __PSP__
    VertexColor2D* v = static_cast<VertexColor2D*>(sceGuGetMemory(4 * sizeof(VertexColor2D)));
    if (!v) return;

    v[0] = { color, static_cast<short>(x1), static_cast<short>(y1), 0 };
    v[1] = { color, static_cast<short>(x2), static_cast<short>(y2), 0 };
    v[2] = { color, static_cast<short>(x4), static_cast<short>(y4), 0 };
    v[3] = { color, static_cast<short>(x3), static_cast<short>(y3), 0 };

    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_TRIANGLE_STRIP, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 4, nullptr, v);
#endif
}

void Renderer::drawLine(int x1, int y1, int x2, int y2, unsigned int color) {
#ifdef __PSP__
    VertexColor2D* v = static_cast<VertexColor2D*>(sceGuGetMemory(2 * sizeof(VertexColor2D)));
    if (!v) return;

    v[0] = { color, static_cast<short>(x1), static_cast<short>(y1), 0 };
    v[1] = { color, static_cast<short>(x2), static_cast<short>(y2), 0 };

    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_LINES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, nullptr, v);
#endif
}

void Renderer::drawCircle(int cx, int cy, int r, unsigned int color, bool fill) {
    drawEllipse(cx, cy, r, r, color, fill);
}

void Renderer::drawEllipse(int cx, int cy, int rx, int ry, unsigned int color, bool fill) {
    const int segments = 16;
    if (fill) {
#ifdef __PSP__
        VertexColor2D* v = static_cast<VertexColor2D*>(sceGuGetMemory((segments + 2) * sizeof(VertexColor2D)));
        if (!v) return;

        v[0] = { color, static_cast<short>(cx), static_cast<short>(cy), 0 };
        for (int i = 0; i <= segments; ++i) {
            float a = (i * 2.0f * 3.14159265f) / segments;
            short px = static_cast<short>(cx + std::cos(a) * rx);
            short py = static_cast<short>(cy + std::sin(a) * ry);
            v[i + 1] = { color, px, py, 0 };
        }

        sceGuDisable(GU_TEXTURE_2D);
        sceGuDrawArray(GU_TRIANGLE_FAN, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, segments + 2, nullptr, v);
#endif
    } else {
        short px = cx + rx;
        short py = cy;
        for (int i = 1; i <= segments; ++i) {
            float a = (i * 2.0f * 3.14159265f) / segments;
            short nx = static_cast<short>(cx + std::cos(a) * rx);
            short ny = static_cast<short>(cy + std::sin(a) * ry);
            drawLine(px, py, nx, ny, color);
            px = nx; py = ny;
        }
    }
}

void Renderer::drawArc(int cx, int cy, int r, float start_angle, float end_angle, unsigned int color, int thickness) {
    const int steps = 12;
    float da = (end_angle - start_angle) / steps;
    float a = start_angle;
    short px = static_cast<short>(cx + std::cos(a) * r);
    short py = static_cast<short>(cy + std::sin(a) * r);

    for (int i = 1; i <= steps; ++i) {
        a += da;
        short nx = static_cast<short>(cx + std::cos(a) * r);
        short ny = static_cast<short>(cy + std::sin(a) * r);
        drawLine(px, py, nx, ny, color);
        if (thickness > 1) {
            drawLine(px + 1, py, nx + 1, ny, color);
            drawLine(px, py + 1, nx, ny + 1, color);
        }
        px = nx; py = ny;
    }
}

void Renderer::drawPick(int x, int y, unsigned int color) {
    // Little guitar-pick cursor shape
    drawQuad(x - 5, y - 5, x + 5, y - 5, x + 2, y + 6, x - 2, y + 6, color);
    drawQuad(x - 2, y + 6, x + 2, y + 6, x, y + 9, x, y + 9, color);
}

void Renderer::drawStar(int cx, int cy, int r, bool filled) {
    short px[10], py[10];
    for (int i = 0; i < 10; ++i) {
        float a = -1.5707963f + (i * 3.14159265f / 5.0f);
        float cr = (i % 2 == 1) ? r * 0.45f : r;
        px[i] = static_cast<short>(cx + std::cos(a) * cr);
        py[i] = static_cast<short>(cy + std::sin(a) * cr);
    }

    if (filled) {
        for (int i = 0; i < 10; ++i) {
            int next = (i + 1) % 10;
            drawQuad(cx, cy, px[i], py[i], px[next], py[next], cx, cy, COLOR_GOLD);
        }
    } else {
        for (int i = 0; i < 10; ++i) {
            int next = (i + 1) % 10;
            drawLine(px[i], py[i], px[next], py[next], COLOR_RGBA(255, 255, 255, 40));
        }
    }
}

void Renderer::drawButtonIcon(int x, int y, ButtonIcon icon) {
    switch (icon) {
        case ICON_CROSS:
            drawLine(x - 3, y - 3, x + 3, y + 3, COLOR_RGBA(143, 176, 255, 255));
            drawLine(x + 3, y - 3, x - 3, y + 3, COLOR_RGBA(143, 176, 255, 255));
            break;
        case ICON_CIRCLE:
            drawCircle(x, y, 4, COLOR_RGBA(255, 123, 134, 255), false);
            break;
        case ICON_TRIANGLE:
            drawLine(x, y - 4, x + 4, y + 3, COLOR_RGBA(100, 240, 140, 255));
            drawLine(x + 4, y + 3, x - 4, y + 3, COLOR_RGBA(100, 240, 140, 255));
            drawLine(x - 4, y + 3, x, y - 4, COLOR_RGBA(100, 240, 140, 255));
            break;
        case ICON_SQUARE:
            drawRectOutline(x - 3, y - 3, 7, 7, COLOR_RGBA(255, 180, 255, 255));
            break;
        case ICON_DPAD:
            drawLine(x, y - 4, x + 3, y, COLOR_GRAY);
            drawLine(x + 3, y, x - 3, y, COLOR_GRAY);
            drawLine(x - 3, y, x, y - 4, COLOR_GRAY);
            drawLine(x, y + 4, x + 3, y, COLOR_GRAY);
            drawLine(x - 3, y, x, y + 4, COLOR_GRAY);
            break;
        case ICON_DPAD_LR:
            drawLine(x - 4, y, x - 1, y - 3, COLOR_GRAY);
            drawLine(x - 4, y, x - 1, y + 3, COLOR_GRAY);
            drawLine(x + 4, y, x + 1, y - 3, COLOR_GRAY);
            drawLine(x + 4, y, x + 1, y + 3, COLOR_GRAY);
            break;
        case ICON_START:
        case ICON_SELECT:
            drawRoundedRect(x - 6, y - 2, 12, 5, 1, COLOR_DIM);
            break;
    }
}

int Renderer::drawButtonHint(int x, int y, ButtonIcon icon, const std::string& label) {
    drawButtonIcon(x + 5, y + 6, icon);
    drawText(x + 14, y + 3, label, COLOR_GRAY, 1);
    return 18 + getTextWidth(label, 1);
}

void Renderer::drawText(int x, int y, const std::string& text, unsigned int color, int scale) {
    if (!m_font_texture || text.empty()) return;

#ifdef __PSP__
    int len = static_cast<int>(text.length());
    VertexTex2D* v = static_cast<VertexTex2D*>(sceGuGetMemory(len * 6 * sizeof(VertexTex2D)));
    if (!v) return;

    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, 128, 64, 128, m_font_texture);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);

    int cur_x = x;
    int v_idx = 0;

    for (int i = 0; i < len; ++i) {
        unsigned char ch = static_cast<unsigned char>(text[i]);
        if (ch == 0xC2 && i + 1 < len && static_cast<unsigned char>(text[i + 1]) == 0xB7) {
            ch = '-'; // Middle dot · -> -
            i++;
        } else if (ch == 0xE2 && i + 2 < len && static_cast<unsigned char>(text[i + 1]) == 0x80) {
            unsigned char b3 = static_cast<unsigned char>(text[i + 2]);
            if (b3 == 0x93 || b3 == 0x94) {
                ch = '-'; // En-dash, Em-dash
            } else if (b3 == 0x98 || b3 == 0x99) {
                ch = '\''; // Smart single quotes
            } else if (b3 == 0x9C || b3 == 0x9D) {
                ch = '\"'; // Smart double quotes
            } else if (b3 == 0xA2) {
                ch = '-'; // Bullet •
            } else {
                ch = ' ';
            }
            i += 2;
        } else if (ch >= 128) {
            if ((ch & 0xE0) == 0xC0) {
                if (i + 1 < len) i += 1;
            } else if ((ch & 0xF0) == 0xE0) {
                if (i + 2 < len) i += 2;
            } else if ((ch & 0xF8) == 0xF0) {
                if (i + 3 < len) i += 3;
            }
            ch = '?';
        }

        if (ch == ' ') {
            cur_x += 8 * scale;
            continue;
        }

        int col = ch % 16;
        int row = ch / 16;
        short u0 = static_cast<short>(col * 8);
        short v0 = static_cast<short>(row * 8);
        short u1 = static_cast<short>(u0 + 8);
        short v1 = static_cast<short>(v0 + 8);

        short x0 = static_cast<short>(cur_x);
        short y0 = static_cast<short>(y);
        short x1 = static_cast<short>(cur_x + 8 * scale);
        short y1 = static_cast<short>(y + 8 * scale);

        v[v_idx + 0] = { u0, v0, color, x0, y0, 0 };
        v[v_idx + 1] = { u1, v0, color, x1, y0, 0 };
        v[v_idx + 2] = { u0, v1, color, x0, y1, 0 };
        v[v_idx + 3] = { u0, v1, color, x0, y1, 0 };
        v[v_idx + 4] = { u1, v0, color, x1, y0, 0 };
        v[v_idx + 5] = { u1, v1, color, x1, y1, 0 };

        v_idx += 6;
        cur_x += 8 * scale;
    }

    if (v_idx > 0) {
        sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, v_idx, nullptr, v);
    }
    sceGuDisable(GU_TEXTURE_2D);
#endif
}

void Renderer::drawTextCentered(int cx, int y, const std::string& text, unsigned int color, int scale) {
    int w = getTextWidth(text, scale);
    drawText(cx - (w / 2), y, text, color, scale);
}

void Renderer::drawTextRight(int rx, int y, const std::string& text, unsigned int color, int scale) {
    int w = getTextWidth(text, scale);
    drawText(rx - w, y, text, color, scale);
}

void Renderer::drawTextSpaced(int cx, int y, const std::string& text, unsigned int color, int spacing, int scale) {
    int total_w = getTextWidth(text, scale, spacing);
    int px = cx - (total_w / 2);
    for (char c : text) {
        std::string s(1, c);
        drawText(px, y, s, color, scale);
        px += 8 * scale + spacing;
    }
}

int Renderer::getTextWidth(const std::string& text, int scale, int spacing) {
    if (text.empty()) return 0;
    int glyphs = 0;
    int len = static_cast<int>(text.length());
    for (int i = 0; i < len; ++i) {
        unsigned char ch = static_cast<unsigned char>(text[i]);
        if (ch == 0xC2 && i + 1 < len && static_cast<unsigned char>(text[i + 1]) == 0xB7) {
            i++;
        } else if (ch == 0xE2 && i + 2 < len && static_cast<unsigned char>(text[i + 1]) == 0x80) {
            i += 2;
        } else if (ch >= 128) {
            if ((ch & 0xE0) == 0xC0) {
                if (i + 1 < len) i += 1;
            } else if ((ch & 0xF0) == 0xE0) {
                if (i + 2 < len) i += 2;
            } else if ((ch & 0xF8) == 0xF0) {
                if (i + 3 < len) i += 3;
            }
        }
        glyphs++;
    }
    return glyphs * 8 * scale + (glyphs > 0 ? (glyphs - 1) * spacing : 0);
}

void Renderer::updateBackground(float dt, float bpm, bool star_power) {
    float rate = dt * 5.0f;
    for (int i = 0; i < 3; ++i) {
        m_bg_tint[i] += (m_target_tint[i] - m_bg_tint[i]) * rate;
    }

    m_ring_rot += dt * 0.25f;
    m_pulse = std::max(0.0f, m_pulse - dt * 3.5f);
    m_energy = std::max(0.0f, m_energy - dt * 0.8f);

    (void)bpm;
    float bounce = (std::sin(m_ring_rot * 8.0f) * 0.5f + 0.5f) * 0.12f;

    for (int i = 0; i < 40; ++i) {
        m_bars[i] = std::max(0.04f, m_bars[i] - dt * 1.5f);
        m_bars[i] = std::max(m_bars[i], bounce * (0.4f + 0.6f * std::sin(i * 0.8f)));
    }
}

void Renderer::triggerBackgroundHit(int lane, float strength) {
    if (lane < 0 || lane >= 5) return;
    unsigned int col = getLaneColor(lane);
    m_target_tint[0] = col & 0xFF;
    m_target_tint[1] = (col >> 8) & 0xFF;
    m_target_tint[2] = (col >> 16) & 0xFF;

    m_pulse = 1.0f;
    m_energy = std::min(1.0f, m_energy + 0.25f * strength);

    int center = lane * 8 + 4;
    for (int i = 0; i < 40; ++i) {
        float dist = std::abs(i - center);
        float boost = std::max(0.0f, 1.0f - dist / 5.5f) * strength * 0.8f;
        m_bars[i] = std::min(1.0f, m_bars[i] + boost);
    }
}

void Renderer::renderBackground(bool star_power, bool in_game) {
    (void)star_power;
    // 1. Base dark background
    drawRect(0, 0, SCR_WIDTH, SCR_HEIGHT, COLOR_PAGE);

    // 2. Ambient radial glow from center (240, 118)
    unsigned int tint = COLOR_RGBA((int)m_bg_tint[0], (int)m_bg_tint[1], (int)m_bg_tint[2], 25);
    drawCircle(240, 118, 140, tint, true);
    drawCircle(240, 118, 80, tint, true);

    // 3. 40 Equalizer Bars along bottom
    for (int i = 0; i < 40; ++i) {
        int lane = i / 8;
        unsigned int col = getLaneColor(lane);
        int bar_h = static_cast<int>(m_bars[i] * 90.0f);
        if (bar_h < 3) bar_h = 3;

        int bx = i * 12 + 1;
        int by = SCR_HEIGHT - bar_h;

        // Bar body
        unsigned int bar_col = (col & 0x00FFFFFF) | 0x35000000;
        drawRect(bx, by, 10, bar_h, bar_col);

        // Top highlight
        drawRect(bx, by, 10, 2, COLOR_RGBA(255, 255, 255, 120));
    }

    // 4. Five-segment glowing outline
    float cx = 240.0f, cy = 118.0f;
    float r = 78.0f + m_pulse * 8.0f;

    if (in_game) {
        // Aligned symmetrically with the 5 lanes of the 3D note highway
        // Upper arch spanning from Green (left) across Red, Yellow (top center), Blue, to Orange (right)
        // Perfectly matches lane ordering and leaves bottom open for highway receptors
        float seg_span = 3.14159265f / 5.0f;
        for (int i = 0; i < 5; ++i) {
            float a0 = -3.14159265f + i * seg_span + seg_span * 0.08f;
            float a1 = a0 + seg_span * 0.84f;
            unsigned int col = getLaneColor(i);
            drawArc(static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(r), a0, a1, col, 2);
        }
    } else {
        // Ambient spinning ring for menus
        for (int i = 0; i < 5; ++i) {
            float a0 = m_ring_rot + i * (6.2831853f / 5.0f);
            float a1 = a0 + (6.2831853f / 5.0f) * 0.76f;
            unsigned int col = getLaneColor(i);
            drawArc(static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(r), a0, a1, col, 2);
        }
    }
}

void Renderer::drawLogo(int cx, int cy, int scale) {
    // Letter-spaced "PSP HERO" title
    drawTextSpaced(cx, cy, "PSP HERO", COLOR_WHITE, 4 * scale, scale);

    // 5-color fret bar beneath logo
    int bar_w = getTextWidth("PSP HERO", scale, 4 * scale);
    int seg_w = (bar_w - 4 * 3) / 5;
    int start_x = cx - (bar_w / 2);
    int bar_y = cy + 10 * scale;

    for (int i = 0; i < 5; ++i) {
        drawRect(start_x + i * (seg_w + 3), bar_y, seg_w, 3, getLaneColor(i));
    }
}

Texture Renderer::loadTexture(const std::string& filepath, int target_w, int target_h) {
    Texture tex = { 0, 0, 0, nullptr, false };
    int w, h, channels;
    unsigned char* data = stbi_load(filepath.c_str(), &w, &h, &channels, 4);
    if (!data) return tex;

    // Allocate 32-bit buffer resized to target dimensions
    size_t bytes = target_w * target_h * sizeof(uint32_t);
    tex.pixels = static_cast<uint32_t*>(std::malloc(bytes));
    if (!tex.pixels) {
        stbi_image_free(data);
        return tex;
    }

    tex.width = target_w;
    tex.height = target_h;
    tex.pitch = target_w;
    tex.is_valid = true;

    // Bilinear or nearest-neighbor rescale
    for (int y = 0; y < target_h; ++y) {
        int src_y = (y * h) / target_h;
        for (int x = 0; x < target_w; ++x) {
            int src_x = (x * w) / target_w;
            int src_idx = (src_y * w + src_x) * 4;
            uint32_t r = data[src_idx + 0];
            uint32_t g = data[src_idx + 1];
            uint32_t b = data[src_idx + 2];
            uint32_t a = data[src_idx + 3];
            tex.pixels[y * target_w + x] = COLOR_RGBA(r, g, b, a);
        }
    }

    stbi_image_free(data);

#ifdef __PSP__
    sceKernelDcacheWritebackAll();
#endif

    return tex;
}

void Renderer::drawTexture(const Texture& tex, int x, int y, int w, int h) {
    if (!tex.is_valid || !tex.pixels) return;

#ifdef __PSP__
    VertexTex2D* v = static_cast<VertexTex2D*>(sceGuGetMemory(4 * sizeof(VertexTex2D)));
    if (!v) return;

    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, tex.width, tex.height, tex.pitch, tex.pixels);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);

    short u0 = 0, v0 = 0;
    short u1 = static_cast<short>(tex.width);
    short v1 = static_cast<short>(tex.height);

    short x0 = static_cast<short>(x);
    short y0 = static_cast<short>(y);
    short x1 = static_cast<short>(x + w);
    short y1 = static_cast<short>(y + h);

    v[0] = { u0, v0, COLOR_WHITE, x0, y0, 0 };
    v[1] = { u1, v0, COLOR_WHITE, x1, y0, 0 };
    v[2] = { u0, v1, COLOR_WHITE, x0, y1, 0 };
    v[3] = { u1, v1, COLOR_WHITE, x1, y1, 0 };

    sceGuDrawArray(GU_TRIANGLE_STRIP, GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 4, nullptr, v);
    sceGuDisable(GU_TEXTURE_2D);
#endif
}

void Renderer::freeTexture(Texture& tex) {
    if (tex.pixels) {
        std::free(tex.pixels);
        tex.pixels = nullptr;
    }
    tex.is_valid = false;
}

unsigned int Renderer::getLaneColor(int lane) {
    static const unsigned int colors[5] = {
        COLOR_LANE_GREEN,
        COLOR_LANE_RED,
        COLOR_LANE_YELLOW,
        COLOR_LANE_BLUE,
        COLOR_LANE_ORANGE
    };
    if (lane >= 0 && lane < 5) return colors[lane];
    return COLOR_LANE_OPEN;
}

unsigned int Renderer::lerpColor(unsigned int c1, unsigned int c2, float t) {
    int r1 = c1 & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = (c1 >> 16) & 0xFF, a1 = (c1 >> 24) & 0xFF;
    int r2 = c2 & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = (c2 >> 16) & 0xFF, a2 = (c2 >> 24) & 0xFF;
    int r = static_cast<int>(r1 + (r2 - r1) * t);
    int g = static_cast<int>(g1 + (g2 - g1) * t);
    int b = static_cast<int>(b1 + (b2 - b1) * t);
    int a = static_cast<int>(a1 + (a2 - a1) * t);
    return COLOR_RGBA(r, g, b, a);
}
