// Frosted glass for the pause menu (ui.h, Menu::Draw): the paused game blurred once as the
// background, translucent rounded panels and tiles over it, and simple vector icons.
// Drawn in software into the Canvas, so it looks the same on PC, phones and the Switch.
// Included by ui.h after Canvas.
#pragma once
#include <mutex>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Glass
{
// sqrt(x*x + y*y) for the per-pixel shapes: std::hypot guards against overflow these small numbers
// never reach and is a slow library call on the Switch (the HUD took 38 ms a frame there).
inline float Hyp(float x, float y) { return std::sqrt(x * x + y * y); }
// src (with its alpha, times cover/255) over dst; the result is opaque.
inline uint32_t Over(uint32_t dst, uint32_t src, int cover = 255)
{
    const int a = (int)(src >> 24) * cover / 255;
    const int da = (int)(dst >> 24);
    // (Two common cases answered at once, as below would: solid over anything, and anything
    // over nothing.)
    if (a >= 255) return src | 0xFF000000u;
    if (da == 0 && a > 0) return (uint32_t)a << 24 | (src & 0xFFFFFFu);
    if (da == 255 || da == 0 && a == 0)
    {
        // (Over a solid picture, as the menus' frosted game: the colour mixed, still solid.)
        if (a <= 0) return dst | 0xFF000000u;
        const int ia = 255 - a;
        const int r = ((int)(src >> 16 & 255) * a + (int)(dst >> 16 & 255) * ia) / 255;
        const int g = ((int)(src >> 8 & 255) * a + (int)(dst >> 8 & 255) * ia) / 255;
        const int b = ((int)(src & 255) * a + (int)(dst & 255) * ia) / 255;
        return da == 0 ? 0 : 0xFF000000u | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
    }
    // (Over a see-through canvas, as the live companion's over the playing game: what is drawn
    // keeps its own transparency, so a shadow stays a soft shadow and not a black band.)
    if (a <= 0) return dst;
    const int oa = a + da * (255 - a) / 255;
    if (oa <= 0) return 0;
    auto mix = [&](int sh) { return ((int)(src >> sh & 255) * a + (int)(dst >> sh & 255) * da * (255 - a) / 255) / oa; };
    return (uint32_t)oa << 24 | (uint32_t)mix(16) << 16 | (uint32_t)mix(8) << 8 | (uint32_t)mix(0);
}

// Grows the canvas's drawn rectangle (Canvas::Dirty) to take in a box.
inline void Mark(Canvas& c, int x0, int y0, int x1, int y1)
{
    x0 = std::max(0, x0); y0 = std::max(0, y0); x1 = std::min(c.w, x1); y1 = std::min(c.h, y1);
    if (x1 <= x0 || y1 <= y0) return;
    c.x0d = std::min(c.x0d, x0); c.y0d = std::min(c.y0d, y0);
    c.x1d = std::max(c.x1d, x1); c.y1d = std::max(c.y1d, y1);
}

// The game behind the menu: src (w x h, ARGB) made small, blurred, darkened a little and a little
// greyer (so white glass and text read over any scene), then scaled back up into out (opaque).
// Done once when the menu opens: the game is paused, so it never needs doing again.
// The tint over the launcher's panels (its Launcher page): the deep blue unless chosen.
// The tints over the launcher's glass (its Launcher page): 0 the panels, 1 the sidebar, 2 the top bar.
inline uint32_t& Tint(int i) { static uint32_t t[3] = { 0x30101826u, 0x30101826u, 0x30101826u }; return t[i]; }
inline uint32_t& PanelTint() { return Tint(0); }
// The launcher's buttons (Play, Continue, the chosen tab), and the ink that reads on them.
inline uint32_t& Button() { static uint32_t b = 0xFFFFFFFFu; return b; }
inline uint32_t ButtonInk()
{
    const uint32_t b = Button();
    return (0.3f * (b >> 16 & 255) + 0.59f * (b >> 8 & 255) + 0.11f * (b & 255)) > 150 ? 0xFF18222Eu : 0xFFFFFFFFu;
}
// blurPct: how blurred (the launcher's Blur, 0 to 100: about that many pixels at 720 high; -1 as always);
// dimPct: how much darker and greyer (the launcher's Dim; 42 as always).
inline void MakeBackdrop(std::vector<uint32_t>& out, const uint32_t* src, int w, int h, int blurPct = -1, int dimPct = 42)
{
    const float mult = 1.0f - std::max(0, std::min(100, dimPct)) / 100.0f, grey = std::max(0, std::min(100, dimPct)) / 100.0f * 0.6f;
    if (blurPct == 0)
    {
        // (Sharp: only darkened, as the blurred one is.)
        out.resize((size_t)w * h);
        for (size_t i = 0; i < out.size(); i++)
        {
            const uint32_t p = src[i];
            const float r = (float)(p >> 16 & 255), g = (float)(p >> 8 & 255), b = (float)(p & 255);
            const float l = 0.3f * r + 0.59f * g + 0.11f * b;
            auto k = [&](float x) { return (uint32_t)std::clamp((int)((x * (1 - grey) + l * grey) * mult), 0, 255); };
            out[i] = 0xFF000000u | k(r) << 16 | k(g) << 8 | k(b);
        }
        return;
    }
    // (Shrunk by f, then blurred rad each way three times: about rad x f pixels in all. Shrunk at
    // least in half: blurring the whole size is too slow for the Switch.)
    int f = std::max(4, std::min(w, h) / 90), rad0 = 2;
    if (blurPct > 0)
    {
        const float sig = blurPct * std::min(w, h) / 720.0f;
        if (sig < 6) { rad0 = 1; f = std::max(2, (int)std::lround(sig / 1.41f)); }
        else f = std::max(2, (int)std::lround(sig / 2.45f));
    }
    const int sw = std::max(1, w / f), sh = std::max(1, h / f);
    std::vector<float> s((size_t)sw * sh * 3), t(s.size());
    for (int y = 0; y < sh; y++)
        for (int x = 0; x < sw; x++)
        {
            float r = 0, g = 0, b = 0;
            int n = 0;
            for (int yy = y * f; yy < std::min(h, y * f + f); yy += 2)
                for (int xx = x * f; xx < std::min(w, x * f + f); xx += 2)
                {
                    const uint32_t p = src[(size_t)yy * w + xx];
                    r += (float)(p >> 16 & 255); g += (float)(p >> 8 & 255); b += (float)(p & 255); n++;
                }
            float* d = &s[((size_t)y * sw + x) * 3];
            d[0] = r / n; d[1] = g / n; d[2] = b / n;
        }
    const int rad = rad0; // three box blurs each way: close to a gaussian
    for (int pass = 0; pass < 3; pass++)
    {
        for (int y = 0; y < sh; y++)
            for (int x = 0; x < sw; x++)
                for (int k = 0; k < 3; k++)
                {
                    float sum = 0;
                    for (int dx = -rad; dx <= rad; dx++) sum += s[((size_t)y * sw + std::clamp(x + dx, 0, sw - 1)) * 3 + k];
                    t[((size_t)y * sw + x) * 3 + k] = sum / (2 * rad + 1);
                }
        for (int y = 0; y < sh; y++)
            for (int x = 0; x < sw; x++)
                for (int k = 0; k < 3; k++)
                {
                    float sum = 0;
                    for (int dy = -rad; dy <= rad; dy++) sum += t[((size_t)std::clamp(y + dy, 0, sh - 1) * sw + x) * 3 + k];
                    s[((size_t)y * sw + x) * 3 + k] = sum / (2 * rad + 1);
                }
    }
    for (size_t i = 0; i < s.size(); i += 3)
    {
        const float l = 0.3f * s[i] + 0.59f * s[i + 1] + 0.11f * s[i + 2];
        for (int k = 0; k < 3; k++) s[i + k] = (s[i + k] * (1 - grey) + l * grey) * mult;
    }
    // (Scaled back up in whole numbers, each column's two sources and weight worked out once: the
    // live glass does this several times a second, so it must be quick.)
    std::vector<int> small(s.size());
    for (size_t i = 0; i < s.size(); i++) small[i] = std::clamp((int)s[i], 0, 255);
    std::vector<int> cx0(w), cx1(w), cwx(w);
    for (int x = 0; x < w; x++)
    {
        const float fx = std::clamp((x + 0.5f) / f - 0.5f, 0.0f, (float)(sw - 1));
        cx0[x] = (int)fx * 3; cx1[x] = std::min(sw - 1, (int)fx + 1) * 3; cwx[x] = (int)((fx - (int)fx) * 256);
    }
    out.resize((size_t)w * h);
    std::vector<int> row((size_t)sw * 3);
    for (int y = 0; y < h; y++)
    {
        const float fy = std::clamp((y + 0.5f) / f - 0.5f, 0.0f, (float)(sh - 1));
        const int y0 = (int)fy, y1 = std::min(sh - 1, y0 + 1), wy = (int)((fy - y0) * 256);
        const int* r0 = &small[(size_t)y0 * sw * 3];
        const int* r1 = &small[(size_t)y1 * sw * 3];
        // (The row between the two small rows first, then across it.)
        for (int i = 0; i < sw * 3; i++) row[i] = r0[i] * (256 - wy) + r1[i] * wy;
        uint32_t* o = &out[(size_t)y * w];
        for (int x = 0; x < w; x++)
        {
            const int a = cx0[x], b = cx1[x], wx = cwx[x];
            const int r = (row[a] * (256 - wx) + row[b] * wx) >> 16, g = (row[a + 1] * (256 - wx) + row[b + 1] * wx) >> 16,
                      bl = (row[a + 2] * (256 - wx) + row[b + 2] * wx) >> 16;
            o[x] = 0xFF000000u | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)bl;
        }
    }
}

// Frosted glass over the game as it plays (the HUD, the status bar, the COMPANION WINDOW): the parts of
// the screen under them, read back now and then and blurred as the menu's backdrop (MakeBackdrop),
// kept in one picture the size of the screen (Live().px, empty: not ready, the glass stays dark).
// Each piece of glass drawn says where it is (Live().want), so the next read takes only those parts.
struct LiveGlass
{
    std::vector<uint32_t> px;
    int w = 0, h = 0;
    struct Rect { int x, y, w, h; };
    std::vector<Rect> want, wanted; // (this frame's pieces; the last frame's, for the read)
    std::mutex lock; // (the read and the drawing may be on two threads: the picture held while either uses it)
    unsigned generation = 0; // (counted up at each read: what is drawn on the glass is drawn again then)
    Rect lastRead { 0, 0, 0, 0 }; // (the part read last)
    // The live companion is drawn in two layers (main.cpp): what is on the glass, drawn only when
    // it changes, and the frost under it, drawn again whenever the glass is read. While the first
    // is drawn (contentOnly), FrostLive only notes where its frost goes (panels).
    bool contentOnly = false;
    // Glass larger than this (pixels; 0: any) stays dark glass: on the Switch, reading back and
    // frosting the COMPANION WINDOW's panel took over 50 ms each time.
    int maxArea = 0;
    struct Panel { int x, y, w, h; float r; };
    std::vector<Panel> panels;
    bool Ready(const Canvas& c) const { return w == c.w && h == c.h && px.size() == (size_t)w * h; }
};
inline LiveGlass& Live() { static LiveGlass g; return g; }

// The menu's background: the blurred game when it is ready (true), otherwise the game
// darkened as before (false: the first frame, or a renderer that cannot be read back).
inline bool Backdrop(Canvas& c)
{
    Mark(c, 0, 0, c.w, c.h);
    if (c.backdrop.size() == (size_t)c.w * c.h && !c.px.empty())
    {
        std::copy(c.backdrop.begin(), c.backdrop.end(), c.px.begin());
        return true;
    }
    std::fill(c.px.begin(), c.px.end(), 0xC0101418u);
    return false;
}

// How strong the frosted glass's grain is (the launcher's Grain): 0 none, 25 as always, 100 four times.
inline int& Grain() { static int g = 25; return g; }
// Frosted glass: the blurred game under a panel made lighter and more colourful, with a fine grain
// and a soft sheen at the top, rather than a grey tint laid over it. Works on the opaque backdrop
// (Backdrop true), inside a rounded rectangle; elsewhere it does nothing.
inline void Frost(Canvas& c, int x, int y, int w, int h, float r, const std::vector<uint32_t>* from = nullptr)
{
    // from: the picture to frost (the canvas's backdrop), rather than what the canvas holds now: a
    // tile lit again over the page as drawn (Menu::DrawChosenTile) is frosted from the game, not
    // from the frost already there.
    if (w <= 0 || h <= 0) return;
    if (from && from->size() != c.px.size()) from = nullptr;
    Mark(c, x, y, x + w, y + h);
    const float hx = w * 0.5f, hy = h * 0.5f, cx = x + hx, cy = y + hy;
    r = std::min(r, std::min(hx, hy));
    // (In whole numbers, in 256ths, as FrostDraw: to within one step of colour of the floating point.)
    for (int py = std::max(0, y); py < std::min(c.h, y + h); py++)
    {
        uint32_t* row = &c.px[(size_t)py * c.w];
        const uint32_t* src = from ? &(*from)[(size_t)py * c.w] : row;
        const float qy = std::fabs(py + 0.5f - cy) - (hy - r);
        const float t = (py - y) / (float)h;
        const int sheen = (int)((t < 0.45f ? (0.45f - t) / 0.45f * 8.0f : 0.0f) * 256); // (lighter towards the top)
        for (int px = std::max(0, x); px < std::min(c.w, x + w); px++)
        {
            int cover = 255;
            if (qy > 0)
            {
                const float qx = std::fabs(px + 0.5f - cx) - (hx - r);
                if (qx > 0)
                {
                    const float f = std::clamp(r - Hyp(qx, qy) + 0.5f, 0.0f, 1.0f); // (the round corners)
                    if (f <= 0) continue;
                    cover = (int)(f * 255);
                }
            }
            const uint32_t b = src[px];
            const int rr = b >> 16 & 255, gg = b >> 8 & 255, bb = b & 255;
            const int l = (77 * rr + 151 * gg + 28 * bb) >> 8;
            // (A fixed grain from the pixel's place, so it does not shimmer from frame to frame.)
            uint32_t hsh = (uint32_t)px * 374761393u + (uint32_t)py * 668265263u;
            hsh = (hsh ^ (hsh >> 13)) * 1274126177u;
            const int add = sheen + ((int)((hsh >> 24) & 7) * 256 - 896) * Grain() / 25;
            const int ch[3] = { rr, gg, bb };
            uint32_t out = 0xFF000000u;
            for (int k = 0; k < 3; k++)
            {
                // (More colour than the backdrop, the darks lifted a little and the brights held down,
                // so white text reads over any part of the game.)
                int v = 311 * ch[k] - 81 * l + 16 * 256;
                if (v > 112 * 256) v = 112 * 256 + (((v - 112 * 256) * 77) >> 8);
                v = (v + add) >> 8;
                out |= (uint32_t)(v < 0 ? 0 : v > 255 ? 255 : v) << (16 - 8 * k);
            }
            row[px] = cover >= 255 ? out : Over(row[px], out, cover);
        }
    }
}

// Frost from the live backdrop (Live()) onto a transparent canvas, over the game: the blurred game
// under the rounded box, given Frost's colour, grain and sheen; the corners soft-edged in alpha.
// False when there is nothing to frost from yet (the caller draws its dark glass instead).
// The frost itself (FrostLive). clearCorners: the corners outside the rounded box made clear
// (the frost's own layer, drawn again over what it held).
inline void FrostDraw(Canvas& c, int x, int y, int w, int h, float r, bool clearCorners = false)
{
    LiveGlass& g = Live();
    if (!g.Ready(c) || w <= 0 || h <= 0) return;
    Mark(c, x, y, x + w, y + h);
    const float hx = w * 0.5f, hy = h * 0.5f, cx = x + hx, cy = y + hy;
    r = std::min(r, std::min(hx, hy));
    // (Frost's colour in whole numbers, in 256ths: the Switch draws this every time the glass is
    // read again, and its floating point per pixel cost milliseconds. The same picture to within
    // one step of colour.)
    for (int py = std::max(0, y); py < std::min(c.h, y + h); py++)
    {
        const float qy = std::fabs(py + 0.5f - cy) - (hy - r);
        const float t = (py - y) / (float)h;
        const int sheen = (int)((t < 0.45f ? (0.45f - t) / 0.45f * 8.0f : 0.0f) * 256);
        const uint32_t* src = &g.px[(size_t)py * g.w];
        uint32_t* dst = &c.px[(size_t)py * c.w];
        for (int px = std::max(0, x); px < std::min(c.w, x + w); px++)
        {
            int cover = 255;
            if (qy > 0)
            {
                const float qx = std::fabs(px + 0.5f - cx) - (hx - r);
                if (qx > 0)
                {
                    const float f = std::clamp(r - Hyp(qx, qy) + 0.5f, 0.0f, 1.0f);
                    if (f <= 0) { if (clearCorners) dst[px] = 0; continue; }
                    cover = (int)(f * 255);
                }
            }
            const uint32_t b = src[px];
            const int rr = b >> 16 & 255, gg = b >> 8 & 255, bb = b & 255;
            const int l = (77 * rr + 151 * gg + 28 * bb) >> 8;
            uint32_t hsh = (uint32_t)px * 374761393u + (uint32_t)py * 668265263u;
            hsh = (hsh ^ (hsh >> 13)) * 1274126177u;
            const int add = sheen + ((int)((hsh >> 24) & 7) * 256 - 896) * Grain() / 25; // (sheen and a fixed grain)
            const int ch[3] = { rr, gg, bb };
            uint32_t out = 0;
            for (int k = 0; k < 3; k++)
            {
                // (More colour than the game, the darks lifted a little, the brights held down.)
                int v = 311 * ch[k] - 81 * l + 16 * 256;
                if (v > 112 * 256) v = 112 * 256 + (((v - 112 * 256) * 77) >> 8);
                v = (v + add) >> 8;
                out |= (uint32_t)(v < 0 ? 0 : v > 255 ? 255 : v) << (16 - 8 * k);
            }
            dst[px] = out | (uint32_t)cover << 24;
        }
    }
}

inline bool FrostLive(Canvas& c, int x, int y, int w, int h, float r)
{
    LiveGlass& g = Live();
    if (g.maxArea > 0 && (long long)w * h > g.maxArea) return false;
    g.want.push_back({ x, y, w, h });
    if (!g.Ready(c) || w <= 0 || h <= 0) return false;
    if (g.contentOnly) { g.panels.push_back({ x, y, w, h, r }); return true; }
    FrostDraw(c, x, y, w, h, r);
    return true;
}

// A panel lifted off the game: a soft shadow round and below it (outside it only, so the glass
// itself is not darkened), size pixels wide.
inline void Shadow(Canvas& c, int x, int y, int w, int h, float r, float size, int alpha = 0x70)
{
    if (w <= 0 || h <= 0 || size <= 0) return;
    const float drop = size * 0.35f; // (the light from above: the shadow a little lower)
    const int s = (int)std::ceil(size);
    Mark(c, x - s, y - s, x + w + s, y + h + s + (int)drop);
    const float hx = w * 0.5f, hy = h * 0.5f, cx = x + hx, cy = y + hy + drop;
    r = std::min(r, std::min(hx, hy));
    for (int py = std::max(0, y - s); py < std::min(c.h, y + h + s + (int)drop); py++)
    {
        uint32_t* row = &c.px[(size_t)py * c.w];
        const float qy = std::fabs(py + 0.5f - cy) - (hy - r);
        // (Rows beside the panel's straight sides: only the bands left and right of it.)
        const bool sides = py > y + r && py < y + h - r;
        for (int px = std::max(0, x - s); px < std::min(c.w, x + w + s); px++)
        {
            if (sides && px >= x && px < x + w) { px = x + w - 1; continue; }
            const float qx = std::fabs(px + 0.5f - cx) - (hx - r);
            const float d = Hyp(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - r;
            // (Not under the panel itself: its own outline, undisplaced.)
            const float qx0 = std::fabs(px + 0.5f - (x + hx)) - (hx - r), qy0 = std::fabs(py + 0.5f - (y + hy)) - (hy - r);
            const float d0 = Hyp(std::max(qx0, 0.0f), std::max(qy0, 0.0f)) + std::min(std::max(qx0, qy0), 0.0f) - r;
            if (d0 <= 0.5f || d >= size) continue;
            const float t = 1.0f - std::max(0.0f, d) / size;
            row[px] = Over(row[px], (uint32_t)(alpha * t * t) << 24);
        }
    }
}

// The lit rim of raised glass: a bright edge at the top fading as it goes down (light from above),
// and a faint second line just inside it, as the thickness of the glass catching the light.
inline void Rim(Canvas& c, int x, int y, int w, int h, float r, float bw, int topAlpha = 0xD8, int bottomAlpha = 0x38)
{
    if (w <= 0 || h <= 0) return;
    Mark(c, x, y, x + w, y + h);
    const float hx = w * 0.5f, hy = h * 0.5f, cx = x + hx, cy = y + hy;
    r = std::min(r, std::min(hx, hy));
    const float band = bw * 3.5f + 1.0f;
    for (int py = std::max(0, y); py < std::min(c.h, y + h); py++)
    {
        uint32_t* row = &c.px[(size_t)py * c.w];
        const float qy = std::fabs(py + 0.5f - cy) - (hy - r);
        const float t = (py - y) / (float)h;
        const float edgeA = topAlpha + (bottomAlpha - topAlpha) * std::min(1.0f, t * 1.6f);
        const float innerA = std::max(0.0f, 1.0f - t * 3.0f) * 0x30;
        const bool sides = py > y + band + r && py < y + h - band - r;
        for (int px = std::max(0, x); px < std::min(c.w, x + w); px++)
        {
            if (sides && px > x + band && px < x + w - band) { px = x + w - (int)band - 1; continue; }
            const float qx = std::fabs(px + 0.5f - cx) - (hx - r);
            const float d = Hyp(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - r;
            if (d > 0.5f || d < -band) continue;
            const float outer = std::clamp(bw * 0.5f + 0.5f - std::fabs(d + bw * 0.5f), 0.0f, 1.0f);
            const float inner = std::clamp(0.5f + 0.5f - std::fabs(d + bw * 2.6f), 0.0f, 1.0f);
            uint32_t& p = row[px];
            if (outer > 0) p = Over(p, (uint32_t)(edgeA) << 24 | 0xFFFFFF, (int)(outer * 255));
            if (inner > 0 && innerA > 0) p = Over(p, (uint32_t)(innerA) << 24 | 0xFFFFFF, (int)(inner * 255));
        }
    }
}

// Text and anything else drawn with partial alpha made solid, over the opaque blurred game.
inline void Opaque(Canvas& c)
{
    for (int y = c.y0d; y < c.y1d; y++)
        for (int x = c.x0d; x < c.x1d; x++) c.px[(size_t)y * c.w + x] |= 0xFF000000u;
}

// A rounded rectangle of glass: fill (with its alpha) and an optional border bw wide, smooth edged.
inline void Round(Canvas& c, int x, int y, int w, int h, float r, uint32_t fill, uint32_t border = 0, float bw = 1.0f)
{
    if (w <= 0 || h <= 0) return;
    Mark(c, x, y, x + w, y + h);
    const float hx = w * 0.5f, hy = h * 0.5f, cx = x + hx, cy = y + hy;
    r = std::min(r, std::min(hx, hy));
    const bool hasBorder = (border >> 24) != 0;
    const float edgeZone = (hasBorder ? bw : 0.0f) + 1.0f;
    for (int py = std::max(0, y); py < std::min(c.h, y + h); py++)
    {
        uint32_t* row = &c.px[(size_t)py * c.w];
        const float qy = std::fabs(py + 0.5f - cy) - (hy - r);
        for (int px = std::max(0, x); px < std::min(c.w, x + w); px++)
        {
            const float qx = std::fabs(px + 0.5f - cx) - (hx - r);
            uint32_t& p = row[px];
            if (qx < -r - edgeZone || qy < -r - edgeZone) // well inside, away from the edge: plain fill
            {
                if (qx < 0 && qy < 0 && std::max(qx, qy) - r < -edgeZone) { p = Over(p, fill); continue; }
            }
            const float d = Hyp(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - r;
            const float inside = std::clamp(0.5f - d, 0.0f, 1.0f);
            if (inside <= 0) continue;
            p = Over(p, fill, (int)(inside * 255));
            if (hasBorder)
            {
                const float e = std::clamp(bw * 0.5f + 0.5f - std::fabs(d + bw * 0.5f), 0.0f, 1.0f);
                if (e > 0) p = Over(p, border, (int)(e * 255));
            }
        }
    }
}

// A soft white glow round a box (whatever is chosen: a tile, a row, a card, a button). k: the size of
// one design pixel.
// The launcher's accent (its Launcher page): the frame round what is chosen and its glow. White unless chosen.
inline uint32_t& Accent() { static uint32_t a = 0xFFFFFFFFu; return a; }
inline uint32_t ChosenEdge() { return 0xF2000000u | (Accent() & 0xFFFFFFu); }
// A picture of the player's own for the launcher (its Launcher page): the panels, the bars, the buttons, the background.
struct Picture
{
    std::vector<uint32_t> px;
    int w = 0, h = 0;
    uint32_t avg = 0xFF808080u;
};
inline Picture& SurfacePic(int i) { static Picture p[4]; return p[i]; } // 0 the panels, 1 the sidebar, 2 the top bar, 3 the buttons
inline int& SurfaceAlpha(int i) { static int a[4] = { 255, 255, 255, 255 }; return a[i]; }
// The picture covering w x h (cut at the sides or top and bottom), into out.
inline void FitCover(const Picture& p, int w, int h, uint32_t* out)
{
    const float sc = std::max((float)w / p.w, (float)h / p.h), ox = (p.w * sc - w) * 0.5f, oy = (p.h * sc - h) * 0.5f;
    for (int y = 0; y < h; y++)
    {
        const uint32_t* row = &p.px[(size_t)std::min(p.h - 1, (int)((y + oy) / sc)) * p.w];
        for (int x = 0; x < w; x++) out[(size_t)y * w + x] = row[std::min(p.w - 1, (int)((x + ox) / sc))] | 0xFF000000u;
    }
}
// The picture in a rounded box (covering it), at alpha a.
inline void PictureIn(Canvas& c, const Picture& p, int x, int y, int w, int h, float r, int a)
{
    if (p.px.empty() || w <= 0 || h <= 0) return;
    const float sc = std::max((float)w / p.w, (float)h / p.h), ox = (p.w * sc - w) * 0.5f, oy = (p.h * sc - h) * 0.5f;
    const int x0 = std::max(0, x), y0 = std::max(0, y), x1 = std::min(c.w, x + w), y1 = std::min(c.h, y + h);
    for (int yy = y0; yy < y1; yy++)
    {
        const uint32_t* row = &p.px[(size_t)std::min(p.h - 1, (int)((yy - y + oy) / sc)) * p.w];
        const float cy = yy + 0.5f, dy = cy < y + r ? y + r - cy : cy > y + h - r ? cy - (y + h - r) : 0;
        for (int xx = x0; xx < x1; xx++)
        {
            const float cx = xx + 0.5f, dx = cx < x + r ? x + r - cx : cx > x + w - r ? cx - (x + w - r) : 0;
            float cov = 1;
            if (dx > 0 && dy > 0) cov = std::max(0.0f, std::min(1.0f, r - std::sqrt(dx * dx + dy * dy) + 0.5f));
            if (cov <= 0) continue;
            uint32_t& d = c.px[(size_t)yy * c.w + xx];
            d = Over(d, row[std::min(p.w - 1, (int)((xx - x + ox) / sc))] | 0xFF000000u, (int)(a * cov));
        }
    }
    Mark(c, x0, y0, x1, y1);
}
// One of the launcher's surfaces: its picture when it has one, else its colour.
inline void Surface(Canvas& c, int i, int x, int y, int w, int h, float r, uint32_t tint)
{
    if (!SurfacePic(i).px.empty()) PictureIn(c, SurfacePic(i), x, y, w, h, r, SurfaceAlpha(i));
    else Round(c, x, y, w, h, r, tint);
}
inline void Glow(Canvas& c, int x, int y, int w, int h, float r, float k)
{
    for (int g = 3; g >= 1; g--)
    {
        const float e = g * 2.4f * k;
        const uint32_t a = (uint32_t)(0x14 * (4 - g)) << 24;
        Round(c, (int)(x - e), (int)(y - e), (int)(w + 2 * e), (int)(h + 2 * e), r + e, 0, a | (Accent() & 0xFFFFFFu), 2.4f * k);
    }
}

// Signed distances for the icons (in the icon's own units: -1 to 1 across it).
inline float Circle(float u, float v, float cx, float cy, float r) { return Hyp(u - cx, v - cy) - r; }
inline float Box(float u, float v, float cx, float cy, float hx, float hy, float r)
{
    const float qx = std::fabs(u - cx) - hx + r, qy = std::fabs(v - cy) - hy + r;
    return Hyp(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - r;
}
inline float Segment(float u, float v, float ax, float ay, float bx, float by, float r)
{
    const float px = u - ax, py = v - ay, dx = bx - ax, dy = by - ay;
    const float t = std::clamp((px * dx + py * dy) / (dx * dx + dy * dy), 0.0f, 1.0f);
    return Hyp(px - dx * t, py - dy * t) - r;
}
// A triangle pointing right, its left edge at x0, tip at x1, half height hh, centred on cy.
inline float Play(float u, float v, float x0, float x1, float cy, float hh)
{
    const float len = Hyp(x1 - x0, hh);
    const float top = ((v - cy) * (x1 - x0) + (u - x1) * hh) / len;   // the upper slanted edge
    const float bot = (-(v - cy) * (x1 - x0) + (u - x1) * hh) / len;  // the lower slanted edge
    return std::max(std::max(x0 - u, top), bot);
}

// The tiles' icons, then the settings pages' (ui_settings.h): header, row and section icons.
enum Icon
{
    I_PLAY, I_SAVE, I_LOAD, I_CHEATS, I_FAST, I_TROPHY, I_GEAR, I_POWER,
    I_BACK, I_MONITOR, I_SCREENS, I_GLOBE, I_RATIO, I_LAYOUT, I_EXPAND, I_FULLSCREEN, I_GRID, I_LAYERS,
    I_PAD, I_GAUGE, I_WINDOW, I_PEOPLE, I_SPARKLE, I_INFO, I_CHEVRON_L, I_CHEVRON_R, I_CHEVRON_U, I_CHEVRON_D, I_POKEBALL, I_CHIP, I_SPEAKER, I_REWIND,
    I_PAUSE, I_PREV, I_NEXT, I_NOTE, I_CHAT, I_SEARCH, I_SORT, I_HOME, I_BUG,
    I_COUNT
};
// A rhombus centred on (cx, cy), half width a and half height b (close to a true distance).
inline float Rhombus(float u, float v, float cx, float cy, float a, float b)
{
    return (std::fabs(u - cx) / a + std::fabs(v - cy) / b - 1.0f) * (a * b / Hyp(a, b));
}

// One of the icons, centred at (cx, cy), size pixels from its centre to its edge.
inline void DrawIcon(Canvas& c, int icon, float cx, float cy, float size, uint32_t argb)
{
    const float t = 0.12f; // line thickness
    auto sdf = [&](float u, float v) -> float {
        switch (icon)
        {
        case I_PLAY: return Play(u, v, -0.42f, 0.62f, 0.0f, 0.62f) - 0.03f;
        case I_SAVE:
        {
            const float body = std::fabs(Box(u, v, 0, 0, 0.72f, 0.72f, 0.14f)) - t / 2;
            const float label = Box(u, v, 0, 0.36f, 0.4f, 0.2f, 0.04f);
            const float shutter = Box(u, v, -0.05f, -0.5f, 0.32f, 0.14f, 0.03f);
            return std::min(body, std::min(label, shutter));
        }
        case I_LOAD:
        {
            const float folder = std::min(Box(u, v, 0, 0.12f, 0.78f, 0.52f, 0.1f), Box(u, v, -0.4f, -0.42f, 0.36f, 0.14f, 0.08f));
            return std::fabs(folder) - t / 2;
        }
        case I_CHEATS:
        {
            const float page = std::fabs(Box(u, v, 0, 0, 0.56f, 0.74f, 0.12f)) - t / 2;
            const float l1 = Segment(u, v, -0.26f, -0.32f, 0.26f, -0.32f, t / 2);
            const float l2 = Segment(u, v, -0.26f, 0.0f, 0.26f, 0.0f, t / 2);
            const float l3 = Segment(u, v, -0.26f, 0.32f, 0.1f, 0.32f, t / 2);
            return std::min(page, std::min(l1, std::min(l2, l3)));
        }
        case I_FAST: return std::min(Play(u, v, -0.78f, 0.04f, 0, 0.52f), Play(u, v, -0.04f, 0.78f, 0, 0.52f)) - 0.02f;
        case I_TROPHY:
        {
            const float rim = Box(u, v, 0, -0.5f, 0.48f, 0.16f, 0.04f);
            const float bowl = std::max(Circle(u, v, 0, -0.36f, 0.48f), -(v + 0.36f));
            const float handles = std::min(std::fabs(Circle(u, v, -0.5f, -0.3f, 0.2f)) - 0.05f, std::fabs(Circle(u, v, 0.5f, -0.3f, 0.2f)) - 0.05f);
            const float stem = Segment(u, v, 0, 0.1f, 0, 0.5f, 0.08f);
            const float base = Box(u, v, 0, 0.62f, 0.36f, 0.1f, 0.04f);
            return std::min(std::min(rim, bowl), std::min(handles, std::min(stem, base)));
        }
        case I_GEAR:
        {
            float gear = Circle(u, v, 0, 0, 0.56f);
            for (int k = 0; k < 8; k++)
            {
                const float a = k * 0.785398f, ca = std::cos(a), sa = std::sin(a);
                const float ru = u * ca + v * sa, rv = -u * sa + v * ca;
                gear = std::min(gear, Box(ru, rv, 0.66f, 0, 0.16f, 0.14f, 0.03f));
            }
            return std::max(gear, -Circle(u, v, 0, 0, 0.24f));
        }
        case I_POWER:
        {
            const float ring = std::fabs(Circle(u, v, 0, 0.08f, 0.58f)) - t / 2;
            const float gap = Box(u, v, 0, -0.5f, 0.26f, 0.32f, 0.0f);
            const float line = Segment(u, v, 0, -0.78f, 0, -0.06f, t / 2);
            return std::min(std::max(ring, -gap), line);
        }

        case I_BACK:
            return std::min(Segment(u, v, -0.7f, 0, 0.72f, 0, t / 2),
                            std::min(Segment(u, v, -0.7f, 0, -0.16f, -0.54f, t / 2), Segment(u, v, -0.7f, 0, -0.16f, 0.54f, t / 2)));
        case I_MONITOR:
        {
            const float screen = std::fabs(Box(u, v, 0, -0.16f, 0.82f, 0.54f, 0.1f)) - t / 2;
            return std::min(screen, std::min(Segment(u, v, 0, 0.38f, 0, 0.66f, t / 2), Segment(u, v, -0.36f, 0.68f, 0.36f, 0.68f, t / 2)));
        }
        case I_SCREENS:
            return std::min(std::fabs(Box(u, v, 0, -0.42f, 0.74f, 0.32f, 0.08f)), std::fabs(Box(u, v, 0, 0.42f, 0.74f, 0.32f, 0.08f))) - t / 2;
        case I_GLOBE:
        {
            const float ring = std::fabs(Circle(u, v, 0, 0, 0.8f)) - t / 2;
            const float meridian = std::fabs((Hyp(u / 0.36f, v / 0.8f) - 1.0f) * 0.36f) - t / 2;
            float lines = std::min(Segment(u, v, -0.8f, 0, 0.8f, 0, t / 2),
                                   std::min(Segment(u, v, -0.7f, -0.4f, 0.7f, -0.4f, t / 2), Segment(u, v, -0.7f, 0.4f, 0.7f, 0.4f, t / 2)));
            lines = std::max(lines, Circle(u, v, 0, 0, 0.8f));
            return std::min(ring, std::min(meridian, lines));
        }
        case I_RATIO:
        {
            const float frame = std::fabs(Box(u, v, 0, 0, 0.84f, 0.58f, 0.1f)) - t / 2;
            return std::min(frame, Box(u, v, 0, 0, 0.42f, 0.34f, 0.05f));
        }
        case I_LAYOUT:
        {
            const float frame = std::fabs(Box(u, v, 0, 0, 0.8f, 0.68f, 0.1f)) - t / 2;
            return std::min(frame, std::min(Segment(u, v, -0.24f, -0.68f, -0.24f, 0.68f, t / 2), Segment(u, v, -0.8f, -0.06f, -0.24f, -0.06f, t / 2)));
        }
        case I_EXPAND:
        case I_FULLSCREEN:
        {
            const float e = icon == I_EXPAND ? 0.78f : 0.46f, l = icon == I_EXPAND ? 0.36f : 0.24f;
            float d = 1.0f;
            for (int sx = -1; sx <= 1; sx += 2)
                for (int sy = -1; sy <= 1; sy += 2)
                    d = std::min(d, std::min(Segment(u, v, sx * e, sy * e, sx * (e - l), sy * e, t / 2), Segment(u, v, sx * e, sy * e, sx * e, sy * (e - l), t / 2)));
            if (icon == I_FULLSCREEN) d = std::min(d, std::fabs(Box(u, v, 0, 0, 0.84f, 0.74f, 0.1f)) - t / 2);
            return d;
        }
        case I_GRID:
        {
            float d = 1.0f;
            for (int gx = -1; gx <= 1; gx++)
                for (int gy = -1; gy <= 1; gy++) d = std::min(d, Box(u, v, gx * 0.56f, gy * 0.56f, 0.17f, 0.17f, 0.04f));
            return d;
        }
        case I_SEARCH: return std::min(std::fabs(Circle(u, v, -0.14f, -0.14f, 0.5f)) - t / 2, Segment(u, v, 0.24f, 0.24f, 0.74f, 0.74f, t / 2));
        case I_SORT:
        {
            const float up = std::min(Segment(u, v, -0.38f, 0.62f, -0.38f, -0.62f, t / 2), std::min(Segment(u, v, -0.38f, -0.62f, -0.68f, -0.3f, t / 2), Segment(u, v, -0.38f, -0.62f, -0.08f, -0.3f, t / 2)));
            const float dn = std::min(Segment(u, v, 0.38f, -0.62f, 0.38f, 0.62f, t / 2), std::min(Segment(u, v, 0.38f, 0.62f, 0.08f, 0.3f, t / 2), Segment(u, v, 0.38f, 0.62f, 0.68f, 0.3f, t / 2)));
            return std::min(up, dn);
        }
        case I_LAYERS:
        {
            const float top = std::fabs(Rhombus(u, v, 0, -0.34f, 0.8f, 0.36f)) - t / 2;
            float d = top;
            for (float cy : { 0.06f, 0.42f })
                d = std::min(d, std::min(Segment(u, v, -0.8f, cy, 0, cy + 0.36f, t / 2), Segment(u, v, 0, cy + 0.36f, 0.8f, cy, t / 2)));
            return d;
        }
        case I_PAD:
        {
            const float body = std::fabs(Box(u, v, 0, 0.04f, 0.86f, 0.46f, 0.32f)) - t / 2;
            const float cross = std::min(Segment(u, v, -0.56f, 0.04f, -0.2f, 0.04f, t / 2), Segment(u, v, -0.38f, -0.14f, -0.38f, 0.22f, t / 2));
            return std::min(body, std::min(cross, std::min(Circle(u, v, 0.3f, 0.12f, 0.08f), Circle(u, v, 0.5f, -0.06f, 0.08f))));
        }
        case I_GAUGE:
        {
            const float arc = std::max(std::fabs(Circle(u, v, 0, 0.16f, 0.74f)) - t / 2, v - 0.5f);
            return std::min(arc, std::min(Segment(u, v, 0, 0.16f, 0.36f, -0.24f, t / 2), Circle(u, v, 0, 0.16f, 0.11f)));
        }
        case I_WINDOW:
        {
            const float frame = std::fabs(Box(u, v, 0, 0, 0.82f, 0.64f, 0.1f)) - t / 2;
            return std::min(frame, std::min(Segment(u, v, -0.82f, -0.28f, 0.82f, -0.28f, t / 2), Box(u, v, -0.36f, 0.2f, 0.28f, 0.2f, 0.04f)));
        }
        case I_PEOPLE:
        {
            const float head1 = std::fabs(Circle(u, v, -0.24f, -0.34f, 0.24f)) - t / 2;
            const float body1 = std::max(std::fabs(Circle(u, v, -0.24f, 0.62f, 0.5f)) - t / 2, v - 0.62f);
            const float head2 = std::max(std::fabs(Circle(u, v, 0.42f, -0.26f, 0.2f)) - t / 2, -Circle(u, v, -0.24f, -0.34f, 0.36f));
            const float body2 = std::max(std::max(std::fabs(Circle(u, v, 0.46f, 0.62f, 0.4f)) - t / 2, v - 0.62f), 0.1f - u);
            return std::min(std::min(head1, body1), std::min(head2, body2));
        }
        case I_SPARKLE:
        {
            const float big = std::min(Rhombus(u, v, -0.14f, 0.12f, 0.16f, 0.72f), Rhombus(u, v, -0.14f, 0.12f, 0.62f, 0.16f));
            const float small = std::min(Rhombus(u, v, 0.56f, -0.52f, 0.08f, 0.32f), Rhombus(u, v, 0.56f, -0.52f, 0.3f, 0.08f));
            return std::min(big, small);
        }
        case I_INFO:
        {
            const float ring = std::fabs(Circle(u, v, 0, 0, 0.82f)) - t / 2;
            return std::min(ring, std::min(Circle(u, v, 0, -0.38f, 0.1f), Segment(u, v, 0, -0.08f, 0, 0.44f, t / 2)));
        }
        case I_CHEVRON_L: return std::min(Segment(u, v, 0.26f, -0.56f, -0.24f, 0, t * 0.6f), Segment(u, v, -0.24f, 0, 0.26f, 0.56f, t * 0.6f));
        case I_CHEVRON_R: return std::min(Segment(u, v, -0.26f, -0.56f, 0.24f, 0, t * 0.6f), Segment(u, v, 0.24f, 0, -0.26f, 0.56f, t * 0.6f));
        case I_CHEVRON_U: return std::min(Segment(u, v, -0.56f, 0.26f, 0, -0.24f, t * 0.6f), Segment(u, v, 0, -0.24f, 0.56f, 0.26f, t * 0.6f));
        case I_CHEVRON_D: return std::min(Segment(u, v, -0.56f, -0.26f, 0, 0.24f, t * 0.6f), Segment(u, v, 0, 0.24f, 0.56f, -0.26f, t * 0.6f));
        case I_CHIP:
        {
            float d = std::fabs(Box(u, v, 0, 0, 0.52f, 0.52f, 0.1f)) - t / 2;
            d = std::min(d, Box(u, v, 0, 0, 0.2f, 0.2f, 0.03f));
            for (int i = -1; i <= 1; i++)
            {
                const float o = i * 0.3f;
                d = std::min(d, std::min(Segment(u, v, o, -0.86f, o, -0.56f, t / 2), Segment(u, v, o, 0.56f, o, 0.86f, t / 2)));
                d = std::min(d, std::min(Segment(u, v, -0.86f, o, -0.56f, o, t / 2), Segment(u, v, 0.56f, o, 0.86f, o, t / 2)));
            }
            return d;
        }
        case I_SPEAKER:
        {
            const float body = std::min(Box(u, v, -0.56f, 0, 0.18f, 0.24f, 0.04f), std::max(Rhombus(u, v, -0.38f, 0, 0.4f, 0.62f), -(u + 0.38f)));
            const float w1 = std::max(std::fabs(Circle(u, v, -0.1f, 0, 0.42f)) - t / 2, -(u - 0.12f) + std::fabs(v) * 0.0f);
            const float w2 = std::max(std::fabs(Circle(u, v, -0.1f, 0, 0.72f)) - t / 2, -(u - 0.22f));
            return std::min(body, std::min(std::max(w1, std::fabs(v) - 0.36f), std::max(w2, std::fabs(v) - 0.6f)));
        }
        case I_REWIND: return std::min(Play(-u, v, -0.78f, 0.04f, 0, 0.52f), Play(-u, v, -0.04f, 0.78f, 0, 0.52f)) - 0.02f;
        case I_PAUSE: return std::min(Box(u, v, -0.32f, 0, 0.17f, 0.62f, 0.07f), Box(u, v, 0.32f, 0, 0.17f, 0.62f, 0.07f));
        case I_NEXT: return std::min(Play(u, v, -0.6f, 0.36f, 0, 0.56f) - 0.02f, Box(u, v, 0.5f, 0, 0.1f, 0.56f, 0.04f));
        case I_PREV: return std::min(Play(-u, v, -0.6f, 0.36f, 0, 0.56f) - 0.02f, Box(u, v, -0.5f, 0, 0.1f, 0.56f, 0.04f));
        case I_NOTE:
        {
            // (Two notes joined by a beam.)
            const float heads = std::min(Circle(u * 0.85f, v, -0.38f, 0.52f, 0.2f), Circle(u * 0.85f, v, 0.38f, 0.38f, 0.2f));
            const float stems = std::min(Segment(u, v, -0.26f, 0.5f, -0.26f, -0.5f, t / 2), Segment(u, v, 0.62f, 0.36f, 0.62f, -0.66f, t / 2));
            const float beam = std::max(std::fabs((v + 0.58f) + (u + 0.26f) * (0.16f / 0.88f)) - 0.1f, std::max(-0.32f - u, u - 0.68f));
            return std::min(heads, std::min(stems, beam));
        }
        case I_CHAT:
        {
            // (A speech bubble with three dots: the Discord row.)
            const float bubble = std::fabs(Box(u, v, 0, -0.12f, 0.8f, 0.54f, 0.32f)) - t / 2;
            const float tail = std::max(std::min(Segment(u, v, -0.42f, 0.42f, -0.52f, 0.8f, t / 2), Segment(u, v, -0.52f, 0.8f, -0.06f, 0.42f, t / 2)), -(v - 0.4f));
            const float dots = std::min(Circle(u, v, -0.34f, -0.12f, 0.1f), std::min(Circle(u, v, 0, -0.12f, 0.1f), Circle(u, v, 0.34f, -0.12f, 0.1f)));
            return std::min(bubble, std::min(tail, dots));
        }
        case I_HOME:
        {
            // (A house: a pitched roof over walls, with a door.)
            const float roof = std::min(Segment(u, v, -0.84f, -0.02f, 0, -0.76f, t / 2), Segment(u, v, 0, -0.76f, 0.84f, -0.02f, t / 2));
            const float walls = std::min(Segment(u, v, -0.62f, -0.22f, -0.62f, 0.72f, t / 2), std::min(Segment(u, v, 0.62f, -0.22f, 0.62f, 0.72f, t / 2), Segment(u, v, -0.62f, 0.72f, 0.62f, 0.72f, t / 2)));
            const float door = std::min(Segment(u, v, -0.2f, 0.72f, -0.2f, 0.18f, t / 2), std::min(Segment(u, v, -0.2f, 0.18f, 0.2f, 0.18f, t / 2), Segment(u, v, 0.2f, 0.18f, 0.2f, 0.72f, t / 2)));
            return std::min(roof, std::min(walls, door));
        }
        case I_BUG:
        {
            // (A ladybug: an oval back split down the middle, a head with two antennae, and six legs.)
            const float back = std::fabs(Circle(u, v * 1.12f, 0, 0.18f, 0.6f)) - t / 2;
            const float split = Segment(u, v, 0, -0.28f, 0, 0.72f, t * 0.45f);
            const float head = std::fabs(Circle(u, v, 0, -0.68f, 0.19f)) - t / 2;
            const float feelers = std::min(Segment(u, v, -0.1f, -0.82f, -0.32f, -0.98f, t / 2),
                                           Segment(u, v, 0.1f, -0.82f, 0.32f, -0.98f, t / 2));
            float legs = 1.0f;
            for (int i = 0; i < 3; i++)
            {
                const float y = -0.24f + i * 0.32f;
                legs = std::min(legs, std::min(Segment(u, v, -0.42f, y, -0.74f, y - 0.14f + i * 0.14f, t / 2),
                                               Segment(u, v, 0.42f, y, 0.74f, y - 0.14f + i * 0.14f, t / 2)));
            }
            const float spots = std::min(Circle(u, v, -0.22f, 0.06f, 0.09f),
                                         std::min(Circle(u, v, 0.22f, 0.06f, 0.09f), Circle(u, v, 0, 0.44f, 0.08f)));
            return std::min(std::min(back, split), std::min(std::min(head, feelers), std::min(legs, spots)));
        }
        case I_POKEBALL:
        {
            const float ring = std::fabs(Circle(u, v, 0, 0, 0.8f)) - t / 2;
            const float band = std::max(Segment(u, v, -0.8f, 0, 0.8f, 0, t / 2), -Circle(u, v, 0, 0, 0.3f));
            return std::min(ring, std::min(band, std::fabs(Circle(u, v, 0, 0, 0.22f)) - t / 2));
        }
        }
        return 1.0f;
    };
    const int x0 = (int)(cx - size) - 2, y0 = (int)(cy - size) - 2, x1 = (int)(cx + size) + 3, y1 = (int)(cy + size) + 3;
    Mark(c, x0, y0, x1, y1);
    for (int py = std::max(0, y0); py < std::min(c.h, y1); py++)
        for (int px = std::max(0, x0); px < std::min(c.w, x1); px++)
        {
            const float d = sdf((px + 0.5f - cx) / size, (py + 0.5f - cy) / size) * size;
            const float cover = std::clamp(0.5f - d, 0.0f, 1.0f);
            if (cover > 0) { uint32_t& p = c.px[(size_t)py * c.w + px]; p = Over(p, argb, (int)(cover * 255)); }
        }
}
} // namespace Glass
