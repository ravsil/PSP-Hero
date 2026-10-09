#include "highway_renderer.h"
#include "font8x8.h"
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

#define Y_TOP 34.0f
#define Y_STRIKE 224.0f
#define X_CENTER 240.0f
#define WIDTH_TOP 120.0f
#define WIDTH_BOTTOM 210.0f

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

HighwayRenderer::HighwayRenderer()
    : m_draw_buffer(nullptr)
    , m_disp_buffer(reinterpret_cast<void*>(FRAME_SIZE))
    , m_depth_buffer(reinterpret_cast<void*>(FRAME_SIZE * 2))
    , m_font_texture(nullptr)
{
    for (int i = 0; i < MAX_SPARKS; ++i) {
        m_sparks[i].timer = 0.0f;
    }
}

HighwayRenderer::~HighwayRenderer() {
    shutdown();
}

void HighwayRenderer::initFontTexture() {
    // 128 characters: 16 columns x 8 rows of 8x8 glyphs -> 128 x 64 pixels
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

bool HighwayRenderer::init() {
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
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuEnable(GU_BLEND);
    sceGuShadeModel(GU_SMOOTH);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
#endif

    return true;
}

void HighwayRenderer::beginFrame() {
#ifdef __PSP__
    sceGuStart(GU_DIRECT, dlist);
    sceGuClearColor(COLOR_RGBA(12, 12, 16, 255));
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
#endif
    updateSparks(1.0f / 60.0f);
}

void HighwayRenderer::endFrame() {
#ifdef __PSP__
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    m_draw_buffer = sceGuSwapBuffers();
#endif
}

void HighwayRenderer::shutdown() {
#ifdef __PSP__
    sceGuTerm();
#endif
    if (m_font_texture) {
        std::free(m_font_texture);
        m_font_texture = nullptr;
    }
}

float HighwayRenderer::getHighwayWidthAt(float y) const {
    float t = (y - Y_TOP) / (Y_STRIKE - Y_TOP);
    if (t < 0.0f) t = 0.0f;
    return WIDTH_TOP + t * (WIDTH_BOTTOM - WIDTH_TOP);
}

float HighwayRenderer::getLaneCenterXAt(int lane, float y) const {
    float hw = getHighwayWidthAt(y);
    float left = X_CENTER - hw * 0.5f;
    float lane_width = hw / 5.0f;
    return left + (lane + 0.5f) * lane_width;
}

float HighwayRenderer::timeToY(double note_time, double current_time) const {
    const double lookahead = 2.0;
    double dt = note_time - current_time;
    float t = static_cast<float>(dt / lookahead);
    return Y_STRIKE - t * (Y_STRIKE - Y_TOP);
}

void HighwayRenderer::drawRect(int x, int y, int w, int h, unsigned int color) {
#ifdef __PSP__
    sceGuDisable(GU_TEXTURE_2D);
    VertexColor2D* v = reinterpret_cast<VertexColor2D*>(sceGuGetMemory(2 * sizeof(VertexColor2D)));
    if (!v) return;
    v[0].color = color; v[0].x = x;     v[0].y = y;     v[0].z = 0;
    v[1].color = color; v[1].x = x + w; v[1].y = y + h; v[1].z = 0;
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, nullptr, v);
#else
    (void)x; (void)y; (void)w; (void)h; (void)color;
#endif
}

void HighwayRenderer::drawRectOutline(int x, int y, int w, int h, unsigned int color) {
    drawLine(x, y, x + w, y, color);
    drawLine(x + w, y, x + w, y + h, color);
    drawLine(x + w, y + h, x, y + h, color);
    drawLine(x, y + h, x, y, color);
}

void HighwayRenderer::drawLine(int x1, int y1, int x2, int y2, unsigned int color) {
#ifdef __PSP__
    sceGuDisable(GU_TEXTURE_2D);
    VertexColor2D* v = reinterpret_cast<VertexColor2D*>(sceGuGetMemory(2 * sizeof(VertexColor2D)));
    if (!v) return;
    v[0].color = color; v[0].x = x1; v[0].y = y1; v[0].z = 0;
    v[1].color = color; v[1].x = x2; v[1].y = y2; v[1].z = 0;
    sceGuDrawArray(GU_LINES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, nullptr, v);
#else
    (void)x1; (void)y1; (void)x2; (void)y2; (void)color;
#endif
}

void HighwayRenderer::drawQuad(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, unsigned int color) {
#ifdef __PSP__
    sceGuDisable(GU_TEXTURE_2D);
    // Draw quad as two triangles
    VertexColor2D* v = reinterpret_cast<VertexColor2D*>(sceGuGetMemory(6 * sizeof(VertexColor2D)));
    if (!v) return;
    // Tri 1: 1, 2, 3
    v[0].color = color; v[0].x = x1; v[0].y = y1; v[0].z = 0;
    v[1].color = color; v[1].x = x2; v[1].y = y2; v[1].z = 0;
    v[2].color = color; v[2].x = x3; v[2].y = y3; v[2].z = 0;
    // Tri 2: 1, 3, 4
    v[3].color = color; v[3].x = x1; v[3].y = y1; v[3].z = 0;
    v[4].color = color; v[4].x = x3; v[4].y = y3; v[4].z = 0;
    v[5].color = color; v[5].x = x4; v[5].y = y4; v[5].z = 0;
    sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 6, nullptr, v);
#else
    (void)x1; (void)y1; (void)x2; (void)y2; (void)x3; (void)y3; (void)x4; (void)y4; (void)color;
#endif
}

void HighwayRenderer::drawText(int x, int y, const std::string& text, unsigned int color, int scale) {
    if (text.empty() || !m_font_texture) return;
    if (scale <= 0) scale = 1;

#ifdef __PSP__
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, 128, 64, 128, m_font_texture);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);

    size_t len = text.size();
    VertexTex2D* v = reinterpret_cast<VertexTex2D*>(sceGuGetMemory(2 * len * sizeof(VertexTex2D)));
    if (!v) return;

    int cur_x = x;
    int cur_y = y;
    int count = 0;

    for (size_t i = 0; i < len; ++i) {
        char ch = text[i];
        if (ch == '\n') {
            cur_x = x;
            cur_y += 10 * scale;
            continue;
        }
        if (ch < 0 || ch >= 128) ch = '?';

        int col = ch % 16;
        int row = ch / 16;
        short u0 = col * 8;
        short v0 = row * 8;

        v[count * 2].u = u0;
        v[count * 2].v = v0;
        v[count * 2].color = color;
        v[count * 2].x = cur_x;
        v[count * 2].y = cur_y;
        v[count * 2].z = 0;

        v[count * 2 + 1].u = u0 + 8;
        v[count * 2 + 1].v = v0 + 8;
        v[count * 2 + 1].color = color;
        v[count * 2 + 1].x = cur_x + 8 * scale;
        v[count * 2 + 1].y = cur_y + 8 * scale;
        v[count * 2 + 1].z = 0;

        cur_x += 8 * scale;
        count++;
    }

    if (count > 0) {
        sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2 * count, nullptr, v);
    }
#else
    (void)x; (void)y; (void)color; (void)scale;
#endif
}

void HighwayRenderer::triggerHitSpark(int lane) {
    if (lane < 0 || lane >= 5) return;
    for (int i = 0; i < MAX_SPARKS; ++i) {
        if (m_sparks[i].timer <= 0.0f) {
            m_sparks[i].x = getLaneCenterXAt(lane, Y_STRIKE);
            m_sparks[i].y = Y_STRIKE;
            m_sparks[i].timer = 0.2f; // 200ms duration
            static const unsigned int colors[] = {
                COLOR_FRET_GREEN, COLOR_FRET_RED, COLOR_FRET_YELLOW,
                COLOR_FRET_BLUE, COLOR_FRET_ORANGE
            };
            m_sparks[i].color = colors[lane];
            break;
        }
    }
}

void HighwayRenderer::updateSparks(float dt) {
    for (int i = 0; i < MAX_SPARKS; ++i) {
        if (m_sparks[i].timer > 0.0f) {
            m_sparks[i].timer -= dt;
            if (m_sparks[i].timer < 0.0f) m_sparks[i].timer = 0.0f;
        }
    }
}

void HighwayRenderer::renderHighway(double current_time) {
    (void)current_time;

    // Highway roadbed (perspective trapezoid)
    float x1 = X_CENTER - WIDTH_TOP * 0.5f;
    float y1 = Y_TOP;
    float x2 = X_CENTER + WIDTH_TOP * 0.5f;
    float y2 = Y_TOP;
    float x3 = X_CENTER + WIDTH_BOTTOM * 0.5f;
    float y3 = Y_STRIKE + 14.0f;
    float x4 = X_CENTER - WIDTH_BOTTOM * 0.5f;
    float y4 = Y_STRIKE + 14.0f;

    // Dark highway body
    drawQuad(x1, y1, x2, y2, x3, y3, x4, y4, COLOR_RGBA(18, 18, 24, 230));

    // Outer rails (Left and Right)
    drawLine(x1, y1, x4, y4, COLOR_RGBA(200, 200, 220, 255));
    drawLine(x1 - 1, y1, x4 - 2, y4, COLOR_RGBA(80, 80, 100, 200));
    drawLine(x2, y2, x3, y3, COLOR_RGBA(200, 200, 220, 255));
    drawLine(x2 + 1, y2, x3 + 2, y3, COLOR_RGBA(80, 80, 100, 200));

    // 4 Lane Dividers
    for (int lane = 1; lane < 5; ++lane) {
        float lx_top = x1 + lane * (WIDTH_TOP / 5.0f);
        float lx_bot = x4 + lane * (WIDTH_BOTTOM / 5.0f);
        drawLine(lx_top, y1, lx_bot, y4, COLOR_RGBA(60, 60, 75, 180));
    }

    // Strikeline bar
    float sl_w = WIDTH_BOTTOM + 6.0f;
    drawRect(X_CENTER - sl_w * 0.5f, Y_STRIKE - 2, sl_w, 4, COLOR_RGBA(240, 240, 255, 240));
    drawRect(X_CENTER - sl_w * 0.5f, Y_STRIKE - 4, sl_w, 8, COLOR_RGBA(100, 140, 255, 80));
}

void HighwayRenderer::renderReceptors(uint32_t held_frets_mask) {
    static const unsigned int fret_colors[] = {
        COLOR_FRET_GREEN, COLOR_FRET_RED, COLOR_FRET_YELLOW,
        COLOR_FRET_BLUE, COLOR_FRET_ORANGE
    };

    float receptor_w = (WIDTH_BOTTOM / 5.0f) * 0.72f;
    float receptor_h = 10.0f;

    for (int lane = 0; lane < 5; ++lane) {
        float cx = getLaneCenterXAt(lane, Y_STRIKE);
        bool is_held = (held_frets_mask & (1u << lane)) != 0;

        int rx = static_cast<int>(cx - receptor_w * 0.5f);
        int ry = static_cast<int>(Y_STRIKE - receptor_h * 0.5f);

        if (is_held) {
            // Bright filled receptor when held
            drawRect(rx - 2, ry - 2, receptor_w + 4, receptor_h + 4, fret_colors[lane]);
            drawRect(rx, ry, receptor_w, receptor_h, COLOR_WHITE);
        } else {
            // Target receptor ring
            drawRectOutline(rx, ry, receptor_w, receptor_h, fret_colors[lane]);
            drawRectOutline(rx + 2, ry + 2, receptor_w - 4, receptor_h - 4, COLOR_RGBA(100, 100, 100, 180));
        }
    }

    // Render active hit sparks
    for (int i = 0; i < MAX_SPARKS; ++i) {
        if (m_sparks[i].timer > 0.0f) {
            float progress = m_sparks[i].timer / 0.2f;
            int spark_sz = static_cast<int>((1.0f - progress) * 20.0f + 8.0f);
            int sx = static_cast<int>(m_sparks[i].x - spark_sz * 0.5f);
            int sy = static_cast<int>(m_sparks[i].y - spark_sz * 0.5f);
            unsigned int alpha = static_cast<unsigned int>(progress * 255);
            unsigned int spark_col = (m_sparks[i].color & 0x00FFFFFF) | (alpha << 24);
            drawRectOutline(sx, sy, spark_sz, spark_sz, spark_col);
            drawRect(sx + 2, sy + 2, spark_sz - 4, spark_sz - 4, COLOR_RGBA(255, 255, 255, alpha));
        }
    }
}

void HighwayRenderer::renderNotes(const ChartTrack& track, double current_time) {
    const double lookahead = 2.0;
    double start_window = current_time - 0.25;
    double end_window = current_time + lookahead;

    static const unsigned int note_colors[] = {
        COLOR_FRET_GREEN, COLOR_FRET_RED, COLOR_FRET_YELLOW,
        COLOR_FRET_BLUE, COLOR_FRET_ORANGE
    };

    // First pass: render sustain tails (behind note heads)
    for (const auto& note : track.notes) {
        if (note.duration_seconds <= 0.0) continue;
        double note_start = note.time_seconds;
        double note_end = note.time_seconds + note.duration_seconds;

        // Skip if outside visible window
        if (note_end < start_window || note_start > end_window) continue;

        double clamp_start = std::max(note_start, current_time);
        double clamp_end = std::min(note_end, end_window);
        if (clamp_start >= clamp_end) continue;

        float p_start = static_cast<float>((clamp_start - current_time) / lookahead);
        float p_end = static_cast<float>((clamp_end - current_time) / lookahead);
        float y_head = Y_STRIKE - p_start * (Y_STRIKE - Y_TOP);
        float y_tail = Y_STRIKE - p_end * (Y_STRIKE - Y_TOP);

        unsigned int tail_color = (note.lane >= 0 && note.lane < 5) ? note_colors[note.lane] : COLOR_FRET_OPEN;
        // Semi-transparent tail
        tail_color = (tail_color & 0x00FFFFFF) | 0xAA000000;

        if (note.lane == LANE_OPEN) {
            // Open note sustain strip across all lanes
            float w_head = getHighwayWidthAt(y_head) * 0.9f;
            float w_tail = getHighwayWidthAt(y_tail) * 0.9f;
            drawQuad(X_CENTER - w_tail * 0.5f, y_tail,
                     X_CENTER + w_tail * 0.5f, y_tail,
                     X_CENTER + w_head * 0.5f, y_head,
                     X_CENTER - w_head * 0.5f, y_head, tail_color);
        } else if (note.lane >= 0 && note.lane < 5) {
            float x_head = getLaneCenterXAt(note.lane, y_head);
            float x_tail = getLaneCenterXAt(note.lane, y_tail);
            float tail_w_head = (getHighwayWidthAt(y_head) / 5.0f) * 0.35f;
            float tail_w_tail = (getHighwayWidthAt(y_tail) / 5.0f) * 0.35f;

            drawQuad(x_tail - tail_w_tail * 0.5f, y_tail,
                     x_tail + tail_w_tail * 0.5f, y_tail,
                     x_head + tail_w_head * 0.5f, y_head,
                     x_head - tail_w_head * 0.5f, y_head, tail_color);
        }
    }

    // Second pass: render note gems
    for (const auto& note : track.notes) {
        if (note.hit) continue; // Don't draw hit notes
        if (note.time_seconds < start_window || note.time_seconds > end_window) continue;

        float p = static_cast<float>((note.time_seconds - current_time) / lookahead);
        float y = Y_STRIKE - p * (Y_STRIKE - Y_TOP);

        if (note.lane == LANE_OPEN) {
            // Open note: horizontal purple bar spanning all 5 lanes
            float hw = getHighwayWidthAt(y) * 0.95f;
            float bar_h = 7.0f + (1.0f - p) * 4.0f;
            int bx = static_cast<int>(X_CENTER - hw * 0.5f);
            int by = static_cast<int>(y - bar_h * 0.5f);

            drawRect(bx, by, hw, bar_h, COLOR_FRET_OPEN);
            drawRectOutline(bx, by, hw, bar_h, COLOR_WHITE);
            // Inner glowing line
            drawRect(bx + 4, by + 2, hw - 8, bar_h - 4, COLOR_RGBA(240, 160, 255, 255));
        } else if (note.lane >= 0 && note.lane < 5) {
            float cx = getLaneCenterXAt(note.lane, y);
            float lane_w = getHighwayWidthAt(y) / 5.0f;
            float note_w = lane_w * 0.78f;
            float note_h = 6.0f + (1.0f - p) * 5.0f;

            int nx = static_cast<int>(cx - note_w * 0.5f);
            int ny = static_cast<int>(y - note_h * 0.5f);

            unsigned int col = note_colors[note.lane];

            // Outer gem body
            drawRect(nx, ny, note_w, note_h, col);
            drawRectOutline(nx, ny, note_w, note_h, COLOR_WHITE);

            // HOPO or Tap styling
            if (note.is_tap) {
                // Translucent inner core
                drawRect(nx + 3, ny + 2, note_w - 6, note_h - 4, COLOR_RGBA(255, 255, 255, 220));
            } else if (note.is_hopo) {
                // White circular/square core
                drawRect(nx + 4, ny + 2, note_w - 8, note_h - 4, COLOR_WHITE);
            } else {
                // Strum note: dark inner core with colored reflection
                drawRect(nx + 3, ny + 2, note_w - 6, note_h - 4, COLOR_RGBA(40, 40, 40, 255));
                drawRect(nx + 5, ny + 2, note_w - 10, 2, COLOR_WHITE);
            }
        }
    }
}

void HighwayRenderer::renderHUD(const ChartSong& song,
                                const ChartTrack& track,
                                double current_time,
                                const GameplayStats& stats,
                                bool is_paused) {
    char buf[128];

    // --- TOP HEADER BAR ---
    drawRect(0, 0, SCR_WIDTH, 26, COLOR_RGBA(16, 16, 22, 240));
    drawLine(0, 26, SCR_WIDTH, 26, COLOR_RGBA(60, 60, 80, 255));

    // Song Title & Artist
    std::string title_str = song.name;
    if (!song.artist.empty()) title_str += " - " + song.artist;
    drawText(10, 4, title_str, COLOR_GOLD, 1);

    // Current Section
    std::string sec = song.getSectionAt(current_time);
    if (!sec.empty()) {
        std::snprintf(buf, sizeof(buf), "[%s]", sec.c_str());
        drawText(10, 15, buf, COLOR_CYAN, 1);
    } else {
        std::snprintf(buf, sizeof(buf), "%s (%s)", track.difficulty.c_str(), track.instrument.c_str());
        drawText(10, 15, buf, COLOR_GRAY, 1);
    }

    // Charter Name
    if (!song.charter.empty()) {
        std::snprintf(buf, sizeof(buf), "Chart: %s", song.charter.c_str());
        drawText(290, 4, buf, COLOR_RGBA(200, 200, 200, 255), 1);
    }

    // BPM
    double bpm = song.getBPMAt(current_time);
    std::snprintf(buf, sizeof(buf), "%.1f BPM", bpm);
    drawText(400, 15, buf, COLOR_WHITE, 1);

    // --- LEFT SIDEBAR (Time & Progress) ---
    drawRect(6, 36, 114, 184, COLOR_RGBA(16, 16, 24, 200));
    drawRectOutline(6, 36, 114, 184, COLOR_RGBA(50, 50, 70, 220));

    drawText(12, 44, "SONG TIME", COLOR_CYAN, 1);

    int cur_m = static_cast<int>(current_time) / 60;
    int cur_s = static_cast<int>(current_time) % 60;
    int tot_m = static_cast<int>(song.total_duration_seconds) / 60;
    int tot_s = static_cast<int>(song.total_duration_seconds) % 60;
    std::snprintf(buf, sizeof(buf), "%02d:%02d / %02d:%02d", cur_m, cur_s, tot_m, tot_s);
    drawText(12, 58, buf, COLOR_WHITE, 1);

    // Progress bar
    drawRect(12, 74, 100, 6, COLOR_RGBA(40, 40, 50, 255));
    if (song.total_duration_seconds > 0.0) {
        float progress = static_cast<float>(current_time / song.total_duration_seconds);
        if (progress > 1.0f) progress = 1.0f;
        if (progress < 0.0f) progress = 0.0f;
        drawRect(12, 74, static_cast<int>(progress * 100.0f), 6, COLOR_CYAN);
    }

    // Mode badge
    drawText(12, 92, "MODE", COLOR_CYAN, 1);
    if (stats.autoplay) {
        drawRect(12, 104, 98, 16, COLOR_RGBA(30, 90, 40, 240));
        drawText(16, 108, "AUTOPLAY", COLOR_WHITE, 1);
    } else {
        drawRect(12, 104, 98, 16, COLOR_RGBA(90, 40, 30, 240));
        drawText(16, 108, "MANUAL PLAY", COLOR_WHITE, 1);
    }

    // Track difficulty & instrument
    drawText(12, 128, "DIFFICULTY", COLOR_CYAN, 1);
    drawText(12, 140, track.difficulty, COLOR_GOLD, 1);
    drawText(12, 152, track.instrument, COLOR_WHITE, 1);

    // Controls summary in sidebar
    drawText(12, 178, "SELECT: Auto", COLOR_GRAY, 1);
    drawText(12, 192, "L/R: Seek", COLOR_GRAY, 1);
    drawText(12, 206, "START: Pause", COLOR_GRAY, 1);

    // --- RIGHT SIDEBAR (Score, Streak, Accuracy) ---
    drawRect(360, 36, 114, 184, COLOR_RGBA(16, 16, 24, 200));
    drawRectOutline(360, 36, 114, 184, COLOR_RGBA(50, 50, 70, 220));

    drawText(366, 44, "SCORE", COLOR_GOLD, 1);
    std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned int>(stats.score));
    drawText(366, 56, buf, COLOR_WHITE, 2);

    // Multiplier & Streak
    drawText(366, 82, "MULTIPLIER", COLOR_CYAN, 1);
    std::snprintf(buf, sizeof(buf), "x%u", static_cast<unsigned int>(stats.multiplier));
    unsigned int mult_color = (stats.multiplier == 4) ? COLOR_GOLD :
                              (stats.multiplier == 3) ? COLOR_FRET_BLUE :
                              (stats.multiplier == 2) ? COLOR_FRET_YELLOW : COLOR_WHITE;
    drawText(366, 94, buf, mult_color, 2);

    std::snprintf(buf, sizeof(buf), "Streak: %u", static_cast<unsigned int>(stats.streak));
    drawText(366, 116, buf, COLOR_WHITE, 1);
    std::snprintf(buf, sizeof(buf), "Max:    %u", static_cast<unsigned int>(stats.max_streak));
    drawText(366, 128, buf, COLOR_GRAY, 1);

    // Accuracy
    drawText(366, 148, "NOTES HIT", COLOR_CYAN, 1);
    std::snprintf(buf, sizeof(buf), "Hit:  %u", static_cast<unsigned int>(stats.notes_hit));
    drawText(366, 160, buf, COLOR_FRET_GREEN, 1);
    std::snprintf(buf, sizeof(buf), "Miss: %u", static_cast<unsigned int>(stats.notes_missed));
    drawText(366, 172, buf, stats.notes_missed > 0 ? COLOR_FRET_RED : COLOR_GRAY, 1);

    uint32_t total_notes = stats.notes_hit + stats.notes_missed;
    float acc = (total_notes > 0) ? (static_cast<float>(stats.notes_hit) / total_notes) * 100.0f : 100.0f;
    std::snprintf(buf, sizeof(buf), "Acc:  %.1f%%", acc);
    drawText(366, 186, buf, COLOR_WHITE, 1);

    // --- BOTTOM STATUS BAR ---
    drawRect(0, 248, SCR_WIDTH, 24, COLOR_RGBA(16, 16, 22, 240));
    drawLine(0, 248, SCR_WIDTH, 248, COLOR_RGBA(60, 60, 80, 255));

    if (is_paused) {
        drawRect(140, 250, 200, 20, COLOR_RGBA(180, 30, 30, 220));
        drawText(160, 256, "** PAUSED ** [START]", COLOR_WHITE, 1);
    } else {
        // Button mapping helper
        drawText(20, 255, "LEFT:G | DOWN/UP:R | TRIANGLE:Y | CIRCLE:B | CROSS:O | SQ:Open", COLOR_RGBA(180, 180, 200, 255), 1);
    }
}

void HighwayRenderer::render(const ChartSong& song,
                             const ChartTrack& track,
                             double current_time,
                             const GameplayStats& stats,
                             bool is_paused,
                             uint32_t held_frets_mask) {
    renderHighway(current_time);
    renderReceptors(held_frets_mask);
    renderNotes(track, current_time);
    renderHUD(song, track, current_time, stats, is_paused);
}
