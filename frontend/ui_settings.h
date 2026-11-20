// Settings pages of the pause menu (Menu::style 1, ui.h): the template every settings page uses.
//   Header: a back button, the page's icon and its title, over a soft line.
//   Left:   the rows, in sections (K_SECTION, with an icon and a line between sections). A row has
//           an icon, its name and on the right one of: an Off / On pair of pills (K_TOGGLE), a
//           < value > picker (K_PICKER), or > for a row that opens a page (K_LINK).
//   Right:  a preview of the chosen row: its name, what it does, a picture for each choice (made
//           from the paused game: Canvas::sharp, dsTop, dsBottom; never bundled art), the choice
//           that is on marked, and a tip.
// On a portrait screen the preview goes under the rows. Sizes follow the panel (k: one pixel of
// the 1400 x 788 design), so it looks the same on a PC window, a phone and the Switch.
// Included at the end of ui.h.
#pragma once
#include "companion_types.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace SettingsArt
{
// The settings colours: frosted glass (Glass::Frost) with white over it, and white for whatever is
// chosen (the row's edge, the pill that is on, its picture), with dark text on a white pill.
// (Raised glass: the panel a deep tint over the frost, lit at its rim; the rows and the preview
// darker glass set into it, each with its own thin lit edge.)
constexpr uint32_t Panel = 0x38101826, PanelEdge = 0x5CFFFFFF, RowEdge = 0x2EFFFFFF;
// The rows and the preview: dark glass set into the panel over a game; the launcher's Settings pages set it as light as its other pages.
inline uint32_t Row = 0x400A111C;
constexpr uint32_t RowChosen = 0x2EFFFFFF, Glow = 0xFFFFFF, ChosenEdge = 0xFFFFFFFF;
constexpr uint32_t Pill = 0xF7FFFFFF, PillEdge = 0xFFFFFFFF, PillInk = 0xFF0B1018, PillOff = 0x1EFFFFFF, PillOffEdge = 0x33FFFFFF;
constexpr uint32_t Desc = 0xFFD3DAE4, Line = 0x2EFFFFFF;

// Text in pixels (px: the font's size) in either font: the clean font at that size, or the pixel
// font at the nearest whole scale. title: written in title case (labels are kept in capitals).
inline std::string Shown(const std::string& s, bool title)
{
    return title ? CleanFont::Title(UiLang::Tr(s)) : UiLang::Tr(s);
}
// A description written in capitals (as the older pages had them) in sentence case: lower case but
// for each sentence's first letter and the names kept in capitals (FPS, HUD, GBA...).
inline std::string Sentence(const std::string& s)
{
    for (char ch : s) if (ch >= 'a' && ch <= 'z') return s; // (already written in sentence case)
    static const char* const keep[] = { "FPS", "HD", "HUD", "GBA", "DS", "3D", "2D", "PC", "OK", "A", "B", "X", "L", "R", "MB", "GPU", "CPU", "JIT", "LCD", "HP", "TM", "HM" };
    std::string out;
    bool start = true;
    size_t i = 0;
    while (i < s.size())
    {
        size_t j = i;
        while (j < s.size() && ((s[j] >= 'A' && s[j] <= 'Z') || (s[j] >= '0' && s[j] <= '9') || (unsigned char)s[j] >= 0x80)) j++;
        if (j == i) { out += s[i]; if (s[i] == '.' || s[i] == '!' || s[i] == '?' || s[i] == ':') start = true; i++; continue; }
        std::string w = s.substr(i, j - i);
        bool kept = false;
        for (const char* k : keep) if (w == k) kept = true;
        // (Y: the button in English; in the other languages only next to ":" or "/" (Y:, X / Y), being
        // otherwise the Spanish "y" (and).)
        if (w == "Y")
        {
            size_t k = j;
            while (k < s.size() && s[k] == ' ') k++;
            const size_t b = out.find_last_not_of(' ');
            kept = UiLang::lang == 0 || (k < s.size() && (s[k] == ':' || s[k] == '/')) || (b != std::string::npos && out[b] == '/');
        }
        // (The games' and hacks' names, and plurals of the names kept in capitals, as they are written.)
        static const char* const names[][2] = { { "RUBY", "Ruby" }, { "SAPPHIRE", "Sapphire" }, { "EMERALD", "Emerald" }, { "FIRERED", "FireRed" },
                                                 { "LEAFGREEN", "LeafGreen" }, { "ULTRA", "Ultra" }, { "VIOLET", "Violet" }, { "MONOMYTH", "Monomyth" },
                                                 { "TMS", "TMs" }, { "HMS", "HMs" }, { "IVS", "IVs" }, { "EVS", "EVs" } };
        for (const auto& nm : names) if (w == nm[0]) w = nm[1], kept = true;
        if (w == "POKEMON") w = "Pok\xC3\xA9" "mon", kept = true;
        else if (w == "POKEDEX") w = "Pok\xC3\xA9" "dex", kept = true;
        if (!kept) UiLang::LowerLatin(w, start ? 1 : 0);
        out += w;
        start = false;
        i = j;
    }
    return out;
}
inline int PixelScale(int px) { return std::max(1, (px + 4) / 9); }
inline int W(const std::string& s, int px, bool bold = false, bool title = true)
{
    px = UiMinPx(px); // (larger menus: the rows' second lines and values were 6 to 9 pixels)
    if (Menu::UseClean()) return CleanFont::Width(Shown(s, title), px, bold);
    return Canvas::TextWidth(s, PixelScale(px));
}
// (y: the baseline.)
inline void T(Canvas& c, int x, int y, const std::string& s, int px, uint32_t col, bool bold = false, bool title = true)
{
    px = UiMinPx(px);
    if (Menu::UseClean()) CleanFont::Draw(c, x, y, Shown(s, title), px, col, bold);
    else c.Text(x, y - 7 * PixelScale(px), s, PixelScale(px), col);
}
inline void TCentred(Canvas& c, int cx, int y, const std::string& s, int px, uint32_t col, bool bold = false)
{
    T(c, cx - W(s, px, bold) / 2, y, s, px, col, bold);
}
// Sentences (descriptions, tips) cut into lines no wider than maxW.
inline std::vector<std::string> Wrap(const std::string& s, int px, int maxW, size_t maxLines)
{
    std::vector<std::string> lines;
    std::string line;
    const std::string text = UiLang::Tr(s);
    // The pieces a line may break between: words at spaces, and in Japanese and Chinese, which
    // have no spaces, between characters (but not before a closing mark: 。、）...). Cut at spaces
    // only, a Japanese answer was one long line running off the panel.
    struct Unit { std::string t; bool space; };
    std::vector<Unit> units;
    auto cjk = [](uint32_t c) { return (c >= 0x2E80 && c <= 0x9FFF) || (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFF00 && c <= 0xFFEF); };
    auto noStart = [](uint32_t c) { return c == 0x3001 || c == 0x3002 || c == 0x300D || c == 0x300F || c == 0x30FC || c == 0xFF01 || c == 0xFF09 ||
                                           c == 0xFF0C || c == 0xFF0E || c == 0xFF1A || c == 0xFF1B || c == 0xFF1F; };
    {
        std::string cur;
        bool space = false, prevCjk = false;
        for (size_t i = 0; i < text.size();)
        {
            const size_t st = i;
            const uint32_t c = UiLang::Next(text, i);
            if (c == ' ')
            {
                if (!cur.empty()) { units.push_back({ cur, space }); cur.clear(); }
                space = true;
                prevCjk = false;
                continue;
            }
            const bool k = cjk(c);
            if (!cur.empty() && (k || prevCjk) && !noStart(c)) { units.push_back({ cur, space }); cur.clear(); space = false; }
            cur += text.substr(st, i - st);
            prevCjk = k;
        }
        if (!cur.empty()) units.push_back({ cur, space });
    }
    for (const Unit& u : units)
    {
        const std::string tryLine = line.empty() ? u.t : line + (u.space ? " " : "") + u.t;
        if (!line.empty() && W(tryLine, px, false, false) > maxW)
        {
            lines.push_back(line);
            line = u.t;
        }
        else line = tryLine;
    }
    if (!line.empty()) lines.push_back(line);
    if (lines.size() > maxLines)
    {
        lines.resize(maxLines);
        lines.back() += "...";
    }
    return lines;
}

// A soft glow round a box (the chosen row, card or button): glass.h's.
inline void GlowRound(Canvas& c, int x, int y, int w, int h, float r, float k) { Glass::Glow(c, x, y, w, h, r, k); }

// A horizontal line brightest in its middle (under the header).
inline void FadeLine(Canvas& c, int x0, int x1, int y, int thick, uint32_t rgb, int alpha)
{
    if (x1 <= x0) return;
    Glass::Mark(c, x0, y - thick, x1, y + 2 * thick);
    for (int x = std::max(0, x0); x < std::min(c.w, x1); x++)
    {
        const float t = (x - x0) / (float)(x1 - x0);
        const float f = std::pow(std::max(0.0f, 1.0f - std::fabs(t - 0.45f) * 1.9f), 0.7f);
        for (int d = -thick; d < 2 * thick; d++)
        {
            const int yy = y + d;
            if (yy < 0 || yy >= c.h) continue;
            const float fall = d >= 0 && d < thick ? 1.0f : 0.3f; // (a faint glow either side)
            uint32_t& p = c.px[(size_t)yy * c.w + x];
            p = Glass::Over(p, (uint32_t)(alpha * f * fall) << 24 | rgb);
        }
    }
}

inline void Rule(Canvas& c, int x, int y, int w, int h, uint32_t argb) { Glass::Round(c, x, y, w, h, 0, argb); }

// The paused game, as pictures are made of it.
struct Src
{
    const uint32_t* p = nullptr;
    int w = 0, h = 0;
};
inline Src Top(const Canvas& c) { return c.dsTop.size() == 256 * 192 ? Src{ c.dsTop.data(), 256, 192 } : Src{}; }
inline Src Bottom(const Canvas& c) { return c.dsBottom.size() == 256 * 192 ? Src{ c.dsBottom.data(), 256, 192 } : Src{}; }
// The whole picture as the player sees it (or the top screen, if the window could not be read).
inline Src Frame(const Canvas& c)
{
    if (!c.sharp.empty() && c.sharp.size() == (size_t)c.w * c.h) return { c.sharp.data(), c.w, c.h };
    return Top(c);
}

// Part of an image (sx, sy, sw, sh) drawn into a box, smooth or as sharp pixels, its corners
// rounded r. With no image: a plain dark screen with a Poke Ball on it.
inline void Image(Canvas& c, Src s, float sx, float sy, float sw, float sh, int dx, int dy, int dw, int dh, bool smooth, float r)
{
    if (dw <= 0 || dh <= 0) return;
    if (!s.p)
    {
        Glass::Round(c, dx, dy, dw, dh, r, 0xFF2A333F);
        const float size = std::min(dw, dh) * 0.26f;
        Glass::DrawIcon(c, Glass::I_POKEBALL, dx + dw * 0.5f, dy + dh * 0.5f, size, 0xFF8995A3);
        return;
    }
    Glass::Mark(c, dx, dy, dx + dw, dy + dh);
    const float hx = dw * 0.5f, hy = dh * 0.5f, cx = dx + hx, cy = dy + hy;
    r = std::min(r, std::min(hx, hy));
    for (int py = std::max(0, dy); py < std::min(c.h, dy + dh); py++)
    {
        const float v = sy + (py - dy + 0.5f) * sh / dh;
        const float qy = std::fabs(py + 0.5f - cy) - (hy - r);
        for (int px = std::max(0, dx); px < std::min(c.w, dx + dw); px++)
        {
            float cover = 1.0f;
            if (r > 0)
            {
                const float qx = std::fabs(px + 0.5f - cx) - (hx - r);
                if (qx > 0 && qy > 0) cover = std::clamp(r - Glass::Hyp(qx, qy) + 0.5f, 0.0f, 1.0f);
                if (cover <= 0) continue;
            }
            const float u = sx + (px - dx + 0.5f) * sw / dw;
            uint32_t out;
            if (!smooth)
            {
                const int ix = std::clamp((int)u, 0, s.w - 1), iy = std::clamp((int)v, 0, s.h - 1);
                out = s.p[(size_t)iy * s.w + ix] | 0xFF000000u;
            }
            else
            {
                const float fx = std::clamp(u - 0.5f, 0.0f, (float)(s.w - 1)), fy = std::clamp(v - 0.5f, 0.0f, (float)(s.h - 1));
                const int x0 = (int)fx, y0 = (int)fy, x1 = std::min(s.w - 1, x0 + 1), y1 = std::min(s.h - 1, y0 + 1);
                const float tx = fx - x0, ty = fy - y0;
                const uint32_t a = s.p[(size_t)y0 * s.w + x0], b = s.p[(size_t)y0 * s.w + x1];
                const uint32_t c2 = s.p[(size_t)y1 * s.w + x0], d = s.p[(size_t)y1 * s.w + x1];
                out = 0xFF000000u;
                for (int sh2 = 0; sh2 <= 16; sh2 += 8)
                {
                    const float va = (float)(a >> sh2 & 255), vb = (float)(b >> sh2 & 255), vc = (float)(c2 >> sh2 & 255), vd = (float)(d >> sh2 & 255);
                    const float val = (va * (1 - tx) + vb * tx) * (1 - ty) + (vc * (1 - tx) + vd * tx) * ty;
                    out |= (uint32_t)std::clamp((int)(val + 0.5f), 0, 255) << sh2;
                }
            }
            uint32_t& p = c.px[(size_t)py * c.w + px];
            p = cover >= 1.0f ? out : Glass::Over(p, out, (int)(cover * 255));
        }
    }
}
inline void Whole(Canvas& c, Src s, int x, int y, int w, int h, float r) { Image(c, s, 0, 0, (float)s.w, (float)s.h, x, y, w, h, true, r); }

// A box of the shape aspect (w / h) as large as fits in (x, y, w, h), centred.
inline void Fit(float aspect, int x, int y, int w, int h, int& ox, int& oy, int& ow, int& oh)
{
    ow = w;
    oh = (int)(w / aspect);
    if (oh > h) { oh = h; ow = (int)(h * aspect); }
    ox = x + (w - ow) / 2;
    oy = y + (h - oh) / 2;
}
inline void Screen(Canvas& c, Src s, float aspect, int x, int y, int w, int h, float r)
{
    int ox, oy, ow, oh;
    Fit(aspect, x, y, w, h, ox, oy, ow, oh);
    Whole(c, s, ox, oy, ow, oh, r);
}
// The top screen cut to the widescreen shape (as MERGED fills a wide screen with it).
inline void WideTop(Canvas& c, int x, int y, int w, int h, float r)
{
    const Src s = Top(c);
    const float want = (float)w / h, sh = std::min(192.0f, 256.0f / want);
    Image(c, s, 0, (192 - sh) / 2, 256, sh, x, y, w, h, true, r);
}
// The other screen drawn small into a corner of a view, edged so it reads as on top.
inline void Inset(Canvas& c, Src s, int x, int y, int w, int h, float k)
{
    Glass::Round(c, x - (int)k, y - (int)k, w + 2 * (int)k, h + 2 * (int)k, 3 * k, 0xE6FFFFFF);
    Whole(c, s, x, y, w, h, 2 * k);
}

// What CONTROLS' pictures show (set by DrawSettings from the menu): the controller's layout
// (0 Xbox, 1 PlayStation, 2 Nintendo, as Controls::Style), the inputs held now, and a key's name.
inline int& PadStyleNow() { static int s = 0; return s; }
inline uint32_t& PadLiveNow() { static uint32_t m = 0; return m; }
inline float* PadAxesNow() { static float a[6] = {}; return a; }
inline std::string& KeyNow() { static std::string s; return s; }
inline std::vector<std::string>& PadLabelsNow() { static std::vector<std::string> v; return v; }
inline std::vector<Menu::TouchShape>& TouchNow() { static std::vector<Menu::TouchShape> v; return v; }

// CONTROLS waiting for a key or button: a small glass card over the page. Where its two buttons
// were drawn (PromptHit, for taps): 0 CANCEL, 1 CLEAR BINDING.
struct PromptRect { int x = 0, y = 0, w = 0, h = 0; };
inline PromptRect* PromptButtons() { static PromptRect r[2]; return r; }
inline int PromptHit(int x, int y)
{
    for (int i = 0; i < 2; i++)
    {
        const PromptRect& r = PromptButtons()[i];
        if (r.w > 0 && x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return i;
    }
    return -1;
}
// title ("Press a button"), what for and what it is now, a warning (a button another action has,
// pressed once), the buttons' hint; canClear: CLEAR BINDING can be chosen.
inline void Prompt(Canvas& c, int icon, const std::string& title, const std::string& sub, const std::string& warn, const std::string& hint, bool canClear)
{
    const float k = std::max(0.42f, std::min(c.w / 1400.0f, c.h / 788.0f) * UiScale());
    auto K = [&](float v) { return (int)std::lround(v * k); };
    Glass::Round(c, 0, 0, c.w, c.h, 0, 0x66000000); // (the page dimmed behind it)
    const int pw = std::min(c.w - K(32), K(460)), innerW = pw - K(48);
    const auto warnLines = warn.empty() ? std::vector<std::string>() : Wrap(warn, K(16), innerW, 3);
    const int ph = K(232) + (int)warnLines.size() * K(23) + (warnLines.empty() ? 0 : K(4));
    const int px = (c.w - pw) / 2, py = (c.h - ph) / 2;
    Glass::Glow(c, px, py, pw, ph, 20 * k, k);
    Glass::Frost(c, px, py, pw, ph, 20 * k);
    Glass::Round(c, px, py, pw, ph, 20 * k, 0xC8141A23, 0xE6FFFFFF, std::max(1.0f, 1.6f * k)); // (dark enough to read over the page)
    Glass::DrawIcon(c, icon, c.w * 0.5f, py + K(38), 16 * k, Menu::GlassText);
    TCentred(c, c.w / 2, py + K(88), title, K(24), Menu::GlassText, true);
    TCentred(c, c.w / 2, py + K(116), sub, K(16), Desc);
    int y = py + K(146);
    for (const std::string& l : warnLines) { TCentred(c, c.w / 2, y, l, K(16), 0xFFFFD27A); y += K(23); }
    if (!warnLines.empty()) y += K(4);
    const int bw = K(150), bh = K(36), gap = K(14), bx = c.w / 2 - bw - gap / 2;
    const char* const labels[] = { "CANCEL", "CLEAR BINDING" };
    for (int i = 0; i < 2; i++)
    {
        const bool off = i == 1 && !canClear;
        const int x = bx + i * (bw + gap);
        Glass::Round(c, x, y, bw, bh, bh * 0.5f, off ? 0x0FFFFFFF : PillOff, off ? 0x1AFFFFFF : PillOffEdge, std::max(1.0f, k));
        TCentred(c, x + bw / 2, y + bh / 2 + K(6), labels[i], K(16), off ? Menu::GlassOff : Menu::GlassText, true);
        PromptButtons()[i] = { x, y, bw, off ? 0 : bh };
    }
    TCentred(c, c.w / 2, y + bh + K(26), hint, K(14), Menu::GlassHint);
}

// Shapes for the controllers, as signed distances in pixels (negative inside).
namespace PadSdf
{
inline float Circle(float x, float y, float cx, float cy, float r) { return Glass::Hyp(x - cx, y - cy) - r; }
inline float Box(float x, float y, float cx, float cy, float hx, float hy, float r)
{
    r = std::min(r, std::min(hx, hy));
    const float qx = std::fabs(x - cx) - hx + r, qy = std::fabs(y - cy) - hy + r;
    return Glass::Hyp(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - r;
}
inline float Capsule(float x, float y, float ax, float ay, float bx, float by, float r)
{
    const float px = x - ax, py = y - ay, dx = bx - ax, dy = by - ay;
    const float t = std::clamp((px * dx + py * dy) / (dx * dx + dy * dy), 0.0f, 1.0f);
    return Glass::Hyp(px - dx * t, py - dy * t) - r;
}
// A smooth join of two shapes (the grips flowing into the body), kk pixels wide.
inline float Smin(float a, float b, float kk)
{
    const float h = std::clamp(0.5f + 0.5f * (b - a) / kk, 0.0f, 1.0f);
    return b * (1 - h) + a * h - kk * h * (1 - h);
}
inline uint32_t Mix(uint32_t a, uint32_t b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    uint32_t out = 0xFF000000u;
    for (int sh = 0; sh <= 16; sh += 8)
        out |= (uint32_t)std::clamp((int)((a >> sh & 255) * (1 - t) + (b >> sh & 255) * t + 0.5f), 0, 255) << sh;
    return out;
}
// A shape drawn lit from above: top colour to bottom colour down its height, its top edges
// catching the light and its bottom edges in shade (from which way its edge faces), a thin light
// rim, and (shadow > 0) a soft shadow under it first.
template <class F>
void Shade(Canvas& c, int x0, int y0, int x1, int y1, F&& sdf, uint32_t top, uint32_t bottom, float bevel, float shadow, float k, bool dish = false)
{
    const int pad = (int)(shadow * 2 + 3);
    x0 -= pad; y0 -= pad; x1 += pad; y1 += pad;
    Glass::Mark(c, x0, y0, x1, y1);
    const float h = (float)std::max(1, y1 - y0 - 2 * pad);
    for (int py = std::max(0, y0); py < std::min(c.h, y1); py++)
        for (int px = std::max(0, x0); px < std::min(c.w, x1); px++)
        {
            const float fx = px + 0.5f, fy = py + 0.5f;
            uint32_t& out = c.px[(size_t)py * c.w + px];
            if (shadow > 0)
            {
                const float ds = sdf(fx, fy - shadow * 0.7f);
                if (ds > -shadow && ds < shadow * 1.6f)
                {
                    const float a = 1.0f - std::clamp((ds + shadow * 0.2f) / (shadow * 1.8f), 0.0f, 1.0f);
                    out = Glass::Over(out, (uint32_t)(a * a * 120) << 24);
                }
            }
            const float d = sdf(fx, fy);
            if (d > 1.0f) continue;
            const float cover = std::clamp(0.5f - d, 0.0f, 1.0f);
            // (Which way the edge faces: up is lit, down is in shade.)
            const float gy = sdf(fx, fy + 1.0f) - sdf(fx, fy - 1.0f), gx = sdf(fx + 1.0f, fy) - sdf(fx - 1.0f, fy);
            const float gl = std::max(0.001f, Glass::Hyp(gx, gy));
            const float up = -(gy * 0.85f + gx * 0.35f) / gl; // (light from the top, a little from the left)
            const float rim = std::clamp(1.0f + d / bevel, 0.0f, 1.0f);
            uint32_t col = Mix(top, bottom, (fy - (y0 + pad)) / h);
            const float lift = (dish ? -up : up) * rim;
            col = lift > 0 ? Mix(col, 0xFFFFFFFF, lift * 0.28f) : Mix(col, 0xFF000000, -lift * 0.45f);
            if (d > -1.3f * k) col = Mix(col, 0xFFFFFFFF, 0.22f + std::max(0.0f, up) * 0.3f); // (the thin rim)
            out = cover >= 1.0f ? col : Glass::Over(out, col, (int)(cover * 255));
        }
}
// A shape as line art: filled (fill, may be clear) and outlined (stroke, sw pixels wide), smooth.
template <class F>
void Outline(Canvas& c, float x0, float y0, float x1, float y1, F&& sdf, uint32_t fill, uint32_t stroke, float sw)
{
    const int ix0 = (int)(x0 - sw - 2), iy0 = (int)(y0 - sw - 2), ix1 = (int)(x1 + sw + 3), iy1 = (int)(y1 + sw + 3);
    Glass::Mark(c, ix0, iy0, ix1, iy1);
    for (int py = std::max(0, iy0); py < std::min(c.h, iy1); py++)
        for (int px = std::max(0, ix0); px < std::min(c.w, ix1); px++)
        {
            const float d = sdf(px + 0.5f, py + 0.5f);
            if (d > sw) continue;
            uint32_t& out = c.px[(size_t)py * c.w + px];
            const float in = std::clamp(0.5f - d, 0.0f, 1.0f);
            if (in > 0 && (fill >> 24)) out = Glass::Over(out, fill, (int)(in * 255));
            const float line = std::clamp(sw * 0.5f + 0.5f - std::fabs(d), 0.0f, 1.0f);
            if (line > 0) out = Glass::Over(out, stroke, (int)(line * 255));
        }
}
// A straight line, smooth, w pixels wide.
inline void Stroke(Canvas& c, float ax, float ay, float bx, float by, float w, uint32_t col)
{
    Outline(c, std::min(ax, bx), std::min(ay, by), std::max(ax, bx), std::max(ay, by),
            [&](float px, float py) { return Capsule(px, py, ax, ay, bx, by, w * 0.5f); }, col, 0, 0);
}
} // namespace PadSdf

// A controller as line art (a dark body, its outline and its buttons drawn in light lines), as
// the kind in the player's hands (Controls::Kind): an Xbox-style pad, a PlayStation-style pad, a
// Pro Controller, a pair of Joy-Con, one Joy-Con held sideways, or the Switch held as one with
// the game on its screen. Plain shapes, no maker's design or logo; its buttons named as its maker
// names them. hi: the input lit (Controls::IN_*, -1 none); live: those held now. With labels
// (PadLabelsNow, KEYS AND BUTTONS), what each input does is written at the side, a line to it.
inline void DrawPad(Canvas& c, int x, int y, int w, int h, float k, int kind, int hi, uint32_t live)
{
    using namespace PadSdf;
    // (Controls::IN_* order: A B X Y BACK GUIDE START LS RS LB RB UP DOWN LEFT RIGHT LT RT.)
    enum { A_, B_, X_, Y_, BACK_, GUIDE_, START_, LS_, RS_, LB_, RB_, UP_, DOWN_, LEFT_, RIGHT_, LT_, RT_, INPUTS };
    enum { XB, PS, PRO, JOYS, JOY, HAND };
    static const char* const xbox[] = { "A", "B", "X", "Y", "", "", "", "", "", "LB", "RB", "", "", "", "", "LT", "RT" };
    static const char* const ps[] = { "\x01", "\x02", "\x03", "\x04", "", "", "", "", "", "L1", "R1", "", "", "", "", "L2", "R2" };
    static const char* const nin[] = { "A", "B", "X", "Y", "-", "", "+", "", "", "L", "R", "", "", "", "", "ZL", "ZR" };
    kind = std::clamp(kind, 0, 5);
    const char* const* names = kind == PS ? ps : kind == XB ? xbox : nin;
    const bool nintendo = kind >= PRO;
    const std::vector<std::string>& labels = PadLabelsNow();
    const bool callouts = labels.size() >= (size_t)INPUTS;
    // The drawing in the middle, the labels' columns either side of it.
    static const float aspects[] = { 1.5f, 1.5f, 1.5f, 1.2f, 2.3f, 2.35f };
    const int padW = callouts ? (int)(w * 0.64f) : w, padX = x + (w - padW) / 2;
    int ox, oy, ow, oh;
    Fit(aspects[kind], padX, y + (callouts ? h / 20 : 0), padW, callouts ? h * 9 / 10 : h, ox, oy, ow, oh);
    const float U = (float)ow;
    auto X = [&](float u) { return ox + u * ow; };
    auto Y = [&](float v) { return oy + v * oh; };
    const float sw = std::max(1.2f, 0.007f * U); // (the lines' width)
    const uint32_t ink = 0xE6F2F5F9, faint = 0x99C6CED8, bodyFill = 0xE0111720, bodyLine = 0xFF8B95A1;
    auto lit = [&](int in) { return in == hi; };
    auto held = [&](int in) { return in >= 0 && in < 32 && (live >> in & 1); };
    auto fillOf = [&](int in) -> uint32_t { return lit(in) ? 0xFFFFFFFF : held(in) ? 0x99FFFFFF : 0x14FFFFFF; };
    auto textOf = [&](int in) -> uint32_t { return lit(in) ? 0xFF0B1018 : ink; };
    // Where each input is (for its label's line): its centre and size; -1 not on this controller.
    float ax[INPUTS], ay[INPUTS], ar[INPUTS];
    for (int i = 0; i < INPUTS; i++) ax[i] = -1;
    auto at = [&](int in, float cx, float cy, float r) { ax[in] = cx; ay[in] = cy; ar[in] = r; };
    auto name = [&](int in, float cx, float cy, int px) {
        if (names[in][0]) T(c, (int)cx - W(names[in], px, true, false) / 2, (int)cy + px * 7 / 20, names[in], px, textOf(in), true, false);
    };
    auto button = [&](int in, float u, float v, float rad) {
        const float cx = X(u), cy = Y(v), r = rad * U;
        if (lit(in)) Glass::Glow(c, (int)(cx - r), (int)(cy - r), (int)(2 * r), (int)(2 * r), r, k);
        Outline(c, cx - r, cy - r, cx + r, cy + r, [&](float px, float py) { return Circle(px, py, cx, cy, r); }, fillOf(in), ink, sw);
        name(in, cx, cy, std::max(7, (int)(r * 1.0f)));
        at(in, cx, cy, r);
    };
    auto pill = [&](int in, float u, float v, float bw, float bh) {
        const float cx = X(u), cy = Y(v), hx = bw * U / 2, hy = bh * U / 2;
        if (lit(in)) Glass::Glow(c, (int)(cx - hx), (int)(cy - hy), (int)(2 * hx), (int)(2 * hy), std::min(hx, hy), k);
        Outline(c, cx - hx, cy - hy, cx + hx, cy + hy, [&](float px, float py) { return Box(px, py, cx, cy, hx, hy, std::min(hx, hy)); }, fillOf(in), ink, sw);
        if (names[in][0]) name(in, cx, cy, std::max(7, (int)(std::min(hx, hy) * 1.7f)));
        at(in, cx, cy, std::max(hx, hy));
    };
    auto stick = [&](int in, float u, float v, float rad) { // (two rings: the stick in its well; the cap follows the real stick)
        const float cx = X(u), cy = Y(v), r = rad * U;
        if (lit(in)) Glass::Glow(c, (int)(cx - r), (int)(cy - r), (int)(2 * r), (int)(2 * r), r, k);
        Outline(c, cx - r, cy - r, cx + r, cy + r, [&](float px, float py) { return Circle(px, py, cx, cy, r); }, 0x30000000, ink, sw);
        const float cr = r * 0.72f;
        const float* axes = PadAxesNow() + (in == RS_ ? 2 : 0);
        float dx = std::clamp(axes[0], -1.0f, 1.0f), dy = std::clamp(axes[1], -1.0f, 1.0f);
        const float len = Glass::Hyp(dx, dy);
        if (len > 1) { dx /= len; dy /= len; }
        const float mx = cx + dx * (r - cr) * 0.95f, my = cy + dy * (r - cr) * 0.95f;
        const bool moved = len > 0.25f;
        Outline(c, mx - cr, my - cr, mx + cr, my + cr, [&](float px, float py) { return Circle(px, py, mx, my, cr); },
                moved && !lit(in) ? 0x66FFFFFF : fillOf(in), ink, sw);
        at(in, cx, cy, r);
    };
    auto shoulder = [&](int in, float u, float v, float bw, float bh, float rr) {
        const float cx = X(u), cy = Y(v), hx = bw * U / 2, hy = bh * U / 2;
        if (lit(in)) Glass::Glow(c, (int)(cx - hx), (int)(cy - hy), (int)(2 * hx), (int)(2 * hy), rr * U, k);
        Outline(c, cx - hx, cy - hy, cx + hx, cy + hy, [&](float px, float py) { return Box(px, py, cx, cy, hx, hy, rr * U); },
                lit(in) || held(in) ? fillOf(in) : bodyFill, bodyLine, sw);
        // (A trigger fills from the top as far as it is pulled.)
        const float pull = in == LT_ ? PadAxesNow()[4] : in == RT_ ? PadAxesNow()[5] : 0;
        if (pull > 0.05f && !lit(in))
        {
            const float fh = 2 * hy * std::min(1.0f, pull);
            Glass::Round(c, (int)(cx - hx + sw), (int)(cy - hy + sw), (int)(2 * hx - 2 * sw), (int)std::max(1.0f, fh - 2 * sw), rr * U * 0.6f, 0x80FFFFFF);
        }
        if (names[in][0]) name(in, cx, cy - hy * 0.2f, std::max(7, (int)(hy * 0.85f)));
        at(in, cx, cy - hy * 0.2f, hx);
    };
    auto arrowKey = [&](int in, float cx, float cy, float a, int icon) { // (one of the + pad's four keys)
        if (lit(in)) Glass::Glow(c, (int)(cx - a / 2), (int)(cy - a / 2), (int)a, (int)a, a * 0.25f, k);
        Outline(c, cx - a / 2, cy - a / 2, cx + a / 2, cy + a / 2, [&](float px, float py) { return Box(px, py, cx, cy, a * 0.46f, a * 0.46f, a * 0.2f); },
                fillOf(in), ink, sw);
        Glass::DrawIcon(c, icon, cx, cy, a * 0.22f, lit(in) ? 0xFF0B1018 : ink);
        at(in, cx, cy, a * 0.5f);
    };
    auto dpad = [&](float u, float v, float arm) {
        const float cx = X(u), cy = Y(v), a = arm * U;
        arrowKey(UP_, cx, cy - a, a, Glass::I_CHEVRON_U);
        arrowKey(DOWN_, cx, cy + a, a, Glass::I_CHEVRON_D);
        arrowKey(LEFT_, cx - a, cy, a, Glass::I_CHEVRON_L);
        arrowKey(RIGHT_, cx + a, cy, a, Glass::I_CHEVRON_R);
    };
    auto roundArrows = [&](float u, float v, float d, float rad) { // (Joy-Con's four round arrow buttons)
        const float dv = d * ow / oh;
        const int in[] = { UP_, DOWN_, LEFT_, RIGHT_ }, icon[] = { Glass::I_CHEVRON_U, Glass::I_CHEVRON_D, Glass::I_CHEVRON_L, Glass::I_CHEVRON_R };
        const float du[] = { 0, 0, -d, d }, dvv[] = { -dv, dv, 0, 0 };
        for (int i = 0; i < 4; i++)
        {
            button(in[i], u + du[i], v + dvv[i], rad);
            Glass::DrawIcon(c, icon[i], X(u + du[i]), Y(v + dvv[i]), rad * U * 0.45f, lit(in[i]) ? 0xFF0B1018 : ink);
        }
    };
    auto face = [&](float u, float v, float d, float rad) {
        // SDL names them by place on Xbox and PlayStation pads (A at the bottom) and by label on
        // Nintendo's (A on the right).
        const int bottom = nintendo ? B_ : A_, right = nintendo ? A_ : B_, left = nintendo ? Y_ : X_, top = nintendo ? X_ : Y_;
        const float dv = d * ow / oh;
        button(top, u, v - dv, rad); button(bottom, u, v + dv, rad); button(left, u - d, v, rad); button(right, u + d, v, rad);
    };
    auto body = [&](auto&& sdf) { Outline(c, (float)ox, (float)oy, (float)(ox + ow), (float)(oy + oh), sdf, bodyFill, bodyLine, sw * 1.3f); };
    auto padBody = [&](float bodyV, float bodyHX, float bodyHY, float bodyR, float gripAU, float gripAV, float gripBU, float gripBV, float gripR) {
        body([&](float px, float py) {
            const float b = Box(px, py, X(0.5f), Y(bodyV), bodyHX * U, bodyHY * U, bodyR * U);
            const float g1 = Capsule(px, py, X(gripAU), Y(gripAV), X(gripBU), Y(gripBV), gripR * U);
            const float g2 = Capsule(px, py, X(1 - gripAU), Y(gripAV), X(1 - gripBU), Y(gripBV), gripR * U);
            return Smin(Smin(b, g1, 0.06f * U), g2, 0.06f * U);
        });
    };
    switch (kind)
    {
    case PS: // a rounder pad: both sticks low and level, the + pad and face buttons high, a touch pad between
        shoulder(LT_, 0.24f, 0.09f, 0.15f, 0.08f, 0.035f);
        shoulder(RT_, 0.76f, 0.09f, 0.15f, 0.08f, 0.035f);
        shoulder(LB_, 0.24f, 0.18f, 0.19f, 0.045f, 0.02f);
        shoulder(RB_, 0.76f, 0.18f, 0.19f, 0.045f, 0.02f);
        padBody(0.4f, 0.4f, 0.16f, 0.12f, 0.2f, 0.45f, 0.15f, 0.8f, 0.11f);
        Outline(c, X(0.36f), Y(0.21f), X(0.64f), Y(0.43f), [&](float px, float py) { return Box(px, py, X(0.5f), Y(0.32f), 0.14f * U, 0.1f * oh, 0.025f * U); },
                0xFF05070A, faint, sw);
        dpad(0.2f, 0.42f, 0.04f);
        face(0.8f, 0.42f, 0.064f, 0.032f);
        stick(LS_, 0.36f, 0.67f, 0.062f);
        stick(RS_, 0.64f, 0.67f, 0.062f);
        pill(BACK_, 0.31f, 0.27f, 0.022f, 0.045f);
        pill(START_, 0.69f, 0.27f, 0.022f, 0.045f);
        button(GUIDE_, 0.5f, 0.56f, 0.024f);
        break;
    case JOYS: // two separate halves, held side by side
        shoulder(LT_, 0.25f, 0.06f, 0.2f, 0.06f, 0.03f);
        shoulder(RT_, 0.75f, 0.06f, 0.2f, 0.06f, 0.03f);
        shoulder(LB_, 0.25f, 0.12f, 0.26f, 0.045f, 0.02f);
        shoulder(RB_, 0.75f, 0.12f, 0.26f, 0.045f, 0.02f);
        body([&](float px, float py) {
            return std::min(Box(px, py, X(0.25f), Y(0.56f), 0.17f * U, 0.42f * oh, 0.13f * U), Box(px, py, X(0.75f), Y(0.56f), 0.17f * U, 0.42f * oh, 0.13f * U));
        });
        stick(LS_, 0.25f, 0.36f, 0.075f);
        roundArrows(0.25f, 0.68f, 0.08f, 0.035f);
        face(0.75f, 0.36f, 0.08f, 0.035f);
        stick(RS_, 0.75f, 0.7f, 0.075f);
        pill(BACK_, 0.36f, 0.2f, 0.045f, 0.022f);
        pill(START_, 0.64f, 0.2f, 0.045f, 0.045f);
        button(GUIDE_, 0.68f, 0.88f, 0.026f);
        break;
    case JOY: // one Joy-Con turned sideways: its stick on the left, its four buttons on the right
        shoulder(LB_, 0.28f, 0.14f, 0.18f, 0.035f, 0.015f);
        shoulder(RB_, 0.72f, 0.14f, 0.18f, 0.035f, 0.015f);
        body([&](float px, float py) { return Box(px, py, X(0.5f), Y(0.55f), 0.46f * U, 0.36f * oh, 0.16f * U); });
        stick(LS_, 0.27f, 0.55f, 0.08f);
        face(0.71f, 0.55f, 0.075f, 0.036f);
        pill(START_, 0.47f, 0.3f, 0.035f, 0.035f);
        break;
    case HAND: // the Switch held as one: the game on its screen, a Joy-Con each side
    {
        shoulder(LT_, 0.09f, 0.05f, 0.11f, 0.03f, 0.012f);
        shoulder(RT_, 0.91f, 0.05f, 0.11f, 0.03f, 0.012f);
        shoulder(LB_, 0.1f, 0.09f, 0.13f, 0.025f, 0.012f);
        shoulder(RB_, 0.9f, 0.09f, 0.13f, 0.025f, 0.012f);
        body([&](float px, float py) { return Box(px, py, X(0.5f), Y(0.55f), 0.49f * U, 0.43f * oh, 0.07f * U); });
        const int sx = (int)X(0.2f), sy = (int)Y(0.2f), sw2 = (int)(0.6f * ow), sh2 = (int)(0.7f * oh);
        Whole(c, Frame(c), sx, sy, sw2, sh2, 0.006f * U);
        Outline(c, (float)sx, (float)sy, (float)(sx + sw2), (float)(sy + sh2),
                [&](float px, float py) { return Box(px, py, sx + sw2 * 0.5f, sy + sh2 * 0.5f, sw2 * 0.5f, sh2 * 0.5f, 0.006f * U); }, 0, faint, sw);
        stick(LS_, 0.1f, 0.36f, 0.042f);
        roundArrows(0.1f, 0.68f, 0.04f, 0.018f);
        face(0.9f, 0.36f, 0.037f, 0.018f);
        stick(RS_, 0.9f, 0.7f, 0.042f);
        pill(BACK_, 0.155f, 0.18f, 0.022f, 0.011f);
        pill(START_, 0.845f, 0.18f, 0.022f, 0.022f);
        button(GUIDE_, 0.87f, 0.9f, 0.013f);
        break;
    }
    default: // XB and PRO: the left stick high and the + pad low, the right stick low
        shoulder(LT_, 0.25f, 0.08f, 0.15f, 0.08f, 0.035f);
        shoulder(RT_, 0.75f, 0.08f, 0.15f, 0.08f, 0.035f);
        shoulder(LB_, 0.25f, 0.17f, 0.21f, 0.05f, 0.025f);
        shoulder(RB_, 0.75f, 0.17f, 0.21f, 0.05f, 0.025f);
        if (kind == PRO) padBody(0.4f, 0.38f, 0.17f, 0.1f, 0.22f, 0.45f, 0.18f, 0.8f, 0.12f);
        else padBody(0.41f, 0.37f, 0.17f, 0.15f, 0.23f, 0.47f, 0.2f, 0.8f, 0.13f);
        stick(LS_, 0.28f, 0.39f, 0.064f);
        stick(RS_, 0.62f, 0.64f, 0.064f);
        dpad(0.38f, 0.64f, 0.034f);
        face(0.72f, 0.39f, 0.06f, 0.032f);
        pill(BACK_, 0.42f, 0.33f, kind == PRO ? 0.028f : 0.03f, kind == PRO ? 0.014f : 0.03f);
        pill(START_, 0.58f, 0.33f, 0.03f, 0.03f);
        button(GUIDE_, 0.5f, kind == PRO ? 0.45f : 0.25f, 0.028f);
        break;
    }
    if (!callouts) return;
    // The labels: each input that does something, its label in the column on its side, the
    // labels spread down the column in the order of their inputs, a line from each to its input.
    const int px = std::max(8, std::min((int)(h * 0.05f), (int)(w * 0.032f))), gapY = px + std::max(3, px / 2);
    for (int side = 0; side < 2; side++)
    {
        std::vector<int> list;
        for (int i = 0; i < INPUTS; i++)
            if (ax[i] >= 0 && !labels[i].empty() && ((ax[i] < X(0.5f)) == (side == 0))) list.push_back(i);
        std::sort(list.begin(), list.end(), [&](int a, int b) { return ay[a] < ay[b]; });
        if (list.empty()) continue;
        const int n = (int)list.size();
        // (Each as level with its input as it can be, kept apart and inside the box.)
        std::vector<float> ly(n);
        for (int i = 0; i < n; i++) ly[i] = std::max(ay[list[i]], i ? ly[i - 1] + gapY : (float)(y + px));
        const float over = ly[n - 1] - (y + h - px * 0.6f);
        if (over > 0)
            for (int i = n - 1; i >= 0; i--) ly[i] = std::min(ly[i] - (i == n - 1 ? over : 0), i < n - 1 ? ly[i + 1] - gapY : ly[i]);
        const float colX = side == 0 ? (float)(padX - px * 0.6f) : (float)(padX + padW + px * 0.6f);
        for (int i = 0; i < n; i++)
        {
            const int in = list[i];
            const bool on = lit(in);
            const uint32_t lc = on ? 0xFFFFFFFF : 0x66FFFFFF, tc = on ? 0xFFFFFFFF : 0xFFB9C3CF;
            const float sx = ax[in] + (side == 0 ? -ar[in] : ar[in]), sy = ay[in];
            const float kneeX = side == 0 ? std::min(sx - px * 0.5f, (float)ox - px * 0.3f) : std::max(sx + px * 0.5f, (float)(ox + ow) + px * 0.3f);
            Stroke(c, sx, sy, kneeX, ly[i], on ? sw * 1.4f : sw * 0.8f, lc);
            Stroke(c, kneeX, ly[i], colX, ly[i], on ? sw * 1.4f : sw * 0.8f, lc);
            const int tw = W(labels[in], px, on, false);
            const int tx = side == 0 ? (int)colX - tw - px / 3 : (int)colX + px / 3;
            T(c, tx, (int)ly[i] + px * 7 / 20, labels[in], px, tc, on, false);
        }
    }
}

// A keyboard's key with its name on it.
inline void DrawKey(Canvas& c, int x, int y, int w, int h, float k, const std::string& name)
{
    const int px = std::max(9, (int)(h * 0.2f));
    const int kh = (int)(h * 0.46f), kw = std::min(w - 4, std::max(kh, W(name, px, true) + (int)(h * 0.3f)));
    const int kx = x + (w - kw) / 2, ky = y + (h - kh) / 2;
    Glass::Glow(c, kx, ky, kw, kh, kh * 0.18f, k);
    Glass::Round(c, kx, ky + (int)(kh * 0.08f), kw, kh, kh * 0.18f, 0xFFAEB7C3); // (its side, under it)
    Glass::Round(c, kx, ky, kw, kh - (int)(kh * 0.04f), kh * 0.18f, 0xFFF4F6F9);
    T(c, kx + (kw - W(name, px, true)) / 2, ky + kh / 2 + px * 7 / 20 - (int)(kh * 0.03f), name, px, 0xFF0B1018, true);
}

// One choice's picture (Menu::P_*, art: its variant), in (x, y, w, h).
inline void Picture(Canvas& c, int kind, int art, int x, int y, int w, int h, float k)
{
    const float r = 7 * k;
    if (kind != Menu::P_PAD && kind != Menu::P_KEY) Glass::Round(c, x, y, w, h, r, 0xFF0E131A); // (the controller and key float on the glass)
    const int pad = (int)(4 * k), ix = x + pad, iy = y + pad, iw = w - 2 * pad, ih = h - 2 * pad, gap = (int)(3 * k);
    const float ds = 256.0f / 192.0f;
    switch (kind)
    {
    case Menu::P_LAYOUT: // MERGED, HYBRID, SIDE, VERTICAL, TOP, BOTTOM; 6: HYBRID, the bottom large; 7: HYBRID, AUTO
        if (art == 0)
        {
            WideTop(c, x, y, w, h, r);
            const int bw = (int)(w * 0.32f), bh = (int)(bw / ds);
            Inset(c, Bottom(c), x + w - bw - pad, y + h - bh - pad, bw, bh, k);
        }
        else if (art == 1 || art == 6 || art == 7)
        {
            const Src big = art == 6 ? Bottom(c) : Top(c), small = art == 6 ? Top(c) : Bottom(c);
            Screen(c, big, ds, ix, iy, (int)(iw * 0.7f), ih, 2 * k);
            Screen(c, small, ds, ix + (int)(iw * 0.7f) + gap, iy, iw - (int)(iw * 0.7f) - gap, ih, 2 * k);
            if (art == 7) // (AUTO: the two swap round as the game needs)
            {
                const float cx = ix + iw * 0.85f, cy = iy + ih * 0.82f, s2 = std::max(3.0f, 6 * k);
                Glass::Round(c, (int)(cx - 2.2f * s2), (int)(cy - 1.4f * s2), (int)(4.4f * s2), (int)(2.8f * s2), 1.4f * s2, 0xD0101820);
                Glass::DrawIcon(c, Glass::I_CHEVRON_L, cx - s2, cy, s2, 0xFFFFFFFF);
                Glass::DrawIcon(c, Glass::I_CHEVRON_R, cx + s2, cy, s2, 0xFFFFFFFF);
            }
        }
        else if (art == 2)
        {
            Screen(c, Top(c), ds, ix, iy, iw / 2 - gap / 2, ih, 2 * k);
            Screen(c, Bottom(c), ds, ix + iw / 2 + gap / 2, iy, iw / 2 - gap / 2, ih, 2 * k);
        }
        else if (art == 3)
        {
            Screen(c, Top(c), ds, ix, iy, iw, ih / 2 - gap / 2, 2 * k);
            Screen(c, Bottom(c), ds, ix, iy + ih / 2 + gap / 2, iw, ih / 2 - gap / 2, 2 * k);
        }
        else Screen(c, art == 4 ? Top(c) : Bottom(c), ds, ix, iy, iw, ih, 2 * k);
        break;
    case Menu::P_LARGE: // AUTO, TOP, BOTTOM: the large screen and the other small beside it
    {
        const Src big = art == 2 ? Bottom(c) : Top(c), small = art == 2 ? Top(c) : Bottom(c);
        const int bw = (int)(iw * 0.68f);
        Screen(c, big, ds, ix, iy, bw, ih, 2 * k);
        Screen(c, small, ds, ix + bw + gap, iy, iw - bw - gap, ih, 2 * k);
        if (art == 0) // (AUTO: the two swap round, the touch screen brought forward while it is used)
        {
            const float cx = ix + bw + (iw - bw) * 0.5f, cy = iy + ih * 0.82f, s = std::max(3.0f, 6 * k);
            Glass::Round(c, (int)(cx - 2.2f * s), (int)(cy - 1.4f * s), (int)(4.4f * s), (int)(2.8f * s), 1.4f * s, 0xD0101820);
            Glass::DrawIcon(c, Glass::I_CHEVRON_L, cx - s, cy, s, 0xFFFFFFFF);
            Glass::DrawIcon(c, Glass::I_CHEVRON_R, cx + s, cy, s, 0xFFFFFFFF);
        }
        break;
    }
    case Menu::P_SECOND: // OFF, EXTRA TOUCH, DS SPLIT, DS SWAPPED, COMPANION: the phone's screen over its second display
    {
        const int mh = (int)(ih * 0.54f), sh = ih - mh - gap * 2;
        int mx, my, mw, mh2, sx, sy, sw, sh2;
        Fit(16.0f / 9.0f, ix, iy, iw, mh, mx, my, mw, mh2);
        Fit(ds, ix, iy + mh + gap * 2, iw, sh, sx, sy, sw, sh2);
        if (art == 2) Screen(c, Top(c), ds, mx, my, mw, mh2, 2 * k);
        else if (art == 3) Screen(c, Bottom(c), ds, mx, my, mw, mh2, 2 * k);
        else
        {
            WideTop(c, mx, my, mw, mh2, 2 * k);
            const int bw = (int)(mw * 0.3f), bh = (int)(bw / ds);
            Inset(c, Bottom(c), mx + mw - bw - gap, my + mh2 - bh - gap, bw, bh, k);
        }
        if (art == 1 || art == 2) Whole(c, Bottom(c), sx, sy, sw, sh2, 2 * k);
        else if (art == 3) Whole(c, Top(c), sx, sy, sw, sh2, 2 * k);
        else if (art == 4)
        {
            Glass::Round(c, sx, sy, sw, sh2, 2 * k, 0xFF1A2536);
            for (int i = 0; i < 4; i++)
            {
                const int ry = sy + sh2 * (i + 1) / 5 - (int)(2 * k);
                Glass::Round(c, sx + sw / 8, ry - (int)(2 * k), (int)(5 * k), (int)(5 * k), 2.5f * k, 0xFF8FB6FF);
                Glass::Round(c, sx + sw / 8 + (int)(9 * k), ry - (int)k, sw * (5 - i % 3) / 9, (int)(3 * k), 1.5f * k, 0xFF4E5E74);
            }
        }
        else Glass::Round(c, sx, sy, sw, sh2, 2 * k, 0xFF06080B);
        Glass::Round(c, sx - (int)k, sy - (int)k, sw + 2 * (int)k, sh2 + 2 * (int)k, 3 * k, 0, 0x66FFFFFF, k);
        break;
    }
    case Menu::P_ASPECT: // AUTO, 16:9, 16:10, 3:2, 4:3, in a screen the shape of this one
    {
        static const float ratios[] = { 0, 16.0f / 9, 16.0f / 10, 3.0f / 2, 4.0f / 3 };
        const float a = art <= 0 || art > 4 ? (float)w / h : ratios[art];
        Glass::Round(c, x, y, w, h, r, 0xFF040608);
        int ox, oy, ow, oh;
        Fit(a, x, y, w, h, ox, oy, ow, oh);
        const Src f = Frame(c);
        Image(c, f, 0, 0, (float)f.w, (float)f.h, ox, oy, ow, oh, true, ow == w ? r : 0);
        break;
    }
    case Menu::P_FULLSCREEN: // OFF (a window on the desktop), ON (the whole screen)
        if (art == 1) Whole(c, Frame(c), x, y, w, h, r);
        else
        {
            Glass::Round(c, x, y, w, h, r, 0xFF2C3B50);
            const int wx = x + (int)(w * 0.13f), wy = y + (int)(h * 0.12f), ww = (int)(w * 0.74f), wh = (int)(h * 0.76f), bar = std::max(3, (int)(h * 0.09f));
            Glass::Round(c, wx, wy, ww, bar + (int)(2 * k), 2 * k, 0xFFC9D2DD);
            for (int i = 0; i < 3; i++)
                Glass::Round(c, wx + ww - (i + 1) * bar, wy + bar / 4, bar / 2, bar / 2, bar / 4.0f, 0xFF6A7787);
            Whole(c, Frame(c), wx, wy + bar, ww, wh - bar, 0);
        }
        break;
    case Menu::P_SMOOTH: // OFF (sharp pixels), ON (blended), close up
    case Menu::P_FILTER: // OFF, SCANLINES, LCD GRID, close up
    {
        const Src s = Top(c);
        const float cw = 40, ch = cw * h / w, cx = 128 - cw / 2, cy = 112 - ch / 2;
        Image(c, s, cx, cy, cw, ch, x, y, w, h, kind == Menu::P_SMOOTH && art == 1, r);
        if (kind == Menu::P_FILTER && art > 0 && s.p)
        {
            const float step = w / cw;
            for (float ly = y; ly < y + h; ly += step)
                Rule(c, x + (int)(r * 0.5f), (int)(ly + step * 0.62f), w - (int)r, std::max(1, (int)(step * 0.38f)), art == 1 ? 0x80000000 : 0x60000000);
            if (art == 2)
                for (float lx = x; lx < x + w; lx += step)
                    Rule(c, (int)(lx + step * 0.7f), y + (int)(r * 0.5f), std::max(1, (int)(step * 0.3f)), h - (int)r, 0x60000000);
        }
        break;
    }
    case Menu::P_BATTLE: // CORNER, STRIP, PANEL, (CLASSIC: STRIP's), SUN AND MOON
    {
        Whole(c, Frame(c), x, y, w, h, r);
        static const uint32_t col[] = { 0xF0E2574C, 0xF04C8BE2, 0xF04CC27A, 0xF0E2BE4C };
        if (art == 0)
        {
            const float s = h * 0.1f, cx = x + w - s * 3.2f, cy = y + h - s * 3.2f;
            const float off[4][2] = { { 0, -1.3f }, { 1.3f, 0 }, { 0, 1.3f }, { -1.3f, 0 } };
            for (int i = 0; i < 4; i++)
                Glass::Round(c, (int)(cx + off[i][0] * s - s / 2 * 1.4f), (int)(cy + off[i][1] * s - s / 2 * 1.4f), (int)(s * 1.4f), (int)(s * 1.4f), s * 0.7f, col[i]);
        }
        else if (art == 1)
        {
            const int bh = (int)(h * 0.2f), by = y + h - bh;
            Glass::Round(c, x, by, w, bh, 0, 0xC0101820);
            for (int i = 0; i < 4; i++)
                Glass::Round(c, x + w * (2 + 23 * i) / 96, by + bh / 5, w * 20 / 96, bh * 3 / 5, bh * 0.25f, col[i]);
        }
        else if (art == 3)
        {
            // SUN AND MOON: FIGHT across the top, POKEMON, BAG and RUN under it, in the lower right.
            const int pw = (int)(w * 0.42f), px = x + w - pw - (int)(w * 0.03f), fh = (int)(h * 0.16f), rh = (int)(h * 0.13f);
            const int gap = std::max(1, (int)(h * 0.02f)), py = y + h - fh - rh - gap - (int)(h * 0.05f), cw = (pw - 2 * gap) / 3;
            Glass::Round(c, px, py, pw, fh, fh * 0.25f, 0xF0E95245);
            static const uint32_t row[3] = { 0xF048C783, 0xF0E2B32F, 0xF0509AEA };
            for (int i = 0; i < 3; i++) Glass::Round(c, px + i * (cw + gap), py + fh + gap, cw, rh, rh * 0.3f, row[i]);
        }
        else
        {
            const int pw = (int)(w * 0.3f), px = x + w - pw;
            Glass::Round(c, px, y, pw, h, 0, 0xC0101820);
            for (int i = 0; i < 4; i++)
                Glass::Round(c, px + pw / 8, y + h * (1 + 6 * i) / 25, pw * 3 / 4, h * 5 / 25, h * 0.05f, col[i]);
        }
        break;
    }
    case Menu::P_FPS:
        Whole(c, Frame(c), x, y, w, h, r);
        if (art == 1)
        {
            const int px = std::max(8, (int)(h * 0.14f));
            const int bw = W("60 FPS", px, true, false) + px, bh = px + px / 2;
            Glass::Round(c, x + pad, y + pad, bw, bh, 3 * k, 0xC8000000);
            T(c, x + pad + px / 2, y + pad + bh - px / 3 - 1, "60 FPS", px, 0xFF7CF29A, true, false);
        }
        break;
    case Menu::P_COMPANION:
        Whole(c, Frame(c), x, y, w, h, r);
        if (art == 1)
        {
            const int pw = (int)(w * 0.36f), px = x + w - pw - pad;
            Glass::Round(c, px, y + pad, pw, h - 2 * pad, 4 * k, 0xD8162131, 0x55FFFFFF, k);
            for (int i = 0; i < 4; i++)
            {
                const int ry = y + pad + (h - 2 * pad) * (2 * i + 1) / 8;
                const int d = std::max(3, (int)(h * 0.08f));
                Glass::Round(c, px + pw / 8, ry - d / 2, d, d, d / 2.0f, 0xFF8FB6FF);
                Glass::Round(c, px + pw / 8 + d + (int)(2 * k), ry - d / 4, pw * (4 - i % 2) / 8, std::max(2, d / 2), d / 4.0f, 0xFF52637A);
            }
        }
        break;
    case Menu::P_PARTY:
        Whole(c, Frame(c), x, y, w, h, r);
        if (art == 1)
        {
            const int pw = (int)(w * 0.34f), px = x + w - pw - pad, ph = (int)(h * 0.66f);
            Glass::Round(c, px, y + pad, pw, ph, 3 * k, 0xCC101722);
            static const float hp[] = { 1.0f, 0.8f, 0.62f, 0.95f, 0.2f, 0.7f };
            for (int i = 0; i < 6; i++)
            {
                const int ry = y + pad + (ph * (i + 1)) / 7;
                const int bw = pw - pw / 4;
                Glass::Round(c, px + pw / 8, ry - (int)(2 * k), bw / 2, std::max(1, (int)(1.6f * k)), 0, 0xFFC7CFD8);
                Glass::Round(c, px + pw / 8, ry, bw, std::max(2, (int)(2.4f * k)), 0, 0xFF394653);
                Glass::Round(c, px + pw / 8, ry, (int)(bw * hp[i]), std::max(2, (int)(2.4f * k)), 0, hp[i] < 0.25f ? 0xFFE05050 : 0xFF45C881);
            }
        }
        break;
    case Menu::P_MOTION: // 30 or 60: a film strip of the game moving along, frame by frame
    {
        // (Six frames in a row, the view creeping right as the camera would. At 30 each frame is
        // shown twice, the copy dimmed; at 60 the gap is a new in-between frame, edged in white.)
        Glass::Round(c, x, y, w, h, r, 0xFF101419);
        const Src f = Frame(c);
        const int n = 4, holes = std::max(2, (int)(h * 0.08f)), band = holes * 2;
        for (int i = 0; i < 2 * n; i++) // (the film's sprocket holes, above and below)
        {
            const int hx = x + (int)((i + 0.5f) * w / (2.0f * n)) - holes / 2;
            Glass::Round(c, hx, y + holes / 2, holes, holes, holes * 0.25f, 0x50FFFFFF);
            Glass::Round(c, hx, y + h - holes / 2 - holes, holes, holes, holes * 0.25f, 0x50FFFFFF);
        }
        const int gap = std::max(1, (int)(w * 0.012f)), fy = y + band, fh = h - 2 * band;
        const int fw = (w - (n + 1) * gap) / n;
        for (int i = 0; i < n; i++)
        {
            const bool copy = art == 0 && i % 2 == 1;     // (30: the same frame again)
            const bool between = art == 1 && i % 2 == 1;  // (60: the frame made in between)
            const float step = art == 0 ? (float)(i / 2 * 2) : (float)i;
            const int fx = x + gap + i * (fw + gap);
            if (f.p)
            {
                const float sw = f.w * 0.34f, sh = sw * fh / std::max(1, fw);
                const float sx = f.w * 0.2f + step * f.w * 0.035f, sy = std::max(0.0f, f.h * 0.52f - sh / 2);
                Image(c, f, sx, sy, sw, std::min(sh, (float)f.h), fx, fy, fw, fh, true, 2 * k);
            }
            else Glass::Round(c, fx, fy, fw, fh, 2 * k, 0xFF2A333F);
            if (copy) Glass::Round(c, fx, fy, fw, fh, 2 * k, 0x8C000000);
            if (between) Glass::Round(c, fx, fy, fw, fh, 2 * k, 0, 0xFFFFFFFF, std::max(1.0f, 1.4f * k));
        }
        break;
    }
    case Menu::P_WIDE: // OFF (the DS's own 4:3 world, bars at the sides), ON (the world drawn wide)
        if (art == 1) Whole(c, Frame(c), x, y, w, h, r);
        else
        {
            Glass::Round(c, x, y, w, h, r, 0xFF040608);
            Screen(c, Top(c), ds, x, y, w, h, 0);
        }
        break;
    case Menu::P_RES: // 3D RESOLUTION, close up: 0 the DS's own picture (192 lines, its real pixels);
                      // 100 the picture as it was drawn when the menu opened (the setting then);
                      // 200 another setting, which can only be seen once the game runs again
    {
        // (The same part of the top screen in both: a patch round its middle, in DS pixels.)
        const float pw = 44, ph2 = pw * h / w, dcx = 128, dcy = 104;
        if (art == 0)
        {
            Image(c, Top(c), dcx - pw / 2, dcy - ph2 / 2, pw, ph2, x, y, w, h, false, r);
        }
        else if (art == 100 && !c.sharp.empty() && c.sharp.size() == (size_t)c.w * c.h && c.baseW > 0)
        {
            const float u = c.baseW / 256.0f;
            Image(c, Frame(c), c.baseX + (dcx - pw / 2) * u, c.baseY + (dcy - ph2 / 2) * u, pw * u, ph2 * u, x, y, w, h, true, r);
        }
        else
        {
            Glass::Round(c, x, y, w, h, r, 0xFF141A22);
            const int px = std::max(8, (int)(h * 0.16f));
            const auto lines = Wrap("Shown when you go back to the game", px, w - 2 * pad, 3);
            int ly = y + h / 2 - (int)lines.size() * px * 13 / 20 + px * 7 / 10;
            for (const std::string& l : lines) { TCentred(c, x + w / 2, ly, l, px, 0xFFB9C3CF); ly += px * 13 / 10; }
        }
        break;
    }
    case Menu::P_PAD: DrawPad(c, x + pad, y + pad, w - 2 * pad, h - 2 * pad, k, PadStyleNow(), art, PadLiveNow()); break;
    case Menu::P_KEY: DrawKey(c, x, y, w, h, k, KeyNow()); break;
    case Menu::P_TOUCH: // the game with the on-screen controls where they are now
    {
        Whole(c, Frame(c), x, y, w, h, r);
        Glass::Round(c, x, y, w, h, r, 0x40000000);
        const float sw = std::max(1.0f, 1.2f * k);
        for (const Menu::TouchShape& t : TouchNow())
        {
            const float cx = x + t.x * w, cy = y + t.y * h;
            if (t.w > 0)
            {
                const float hx = t.w * w / 2, hy = t.h * h / 2;
                PadSdf::Outline(c, cx - hx, cy - hy, cx + hx, cy + hy, [&](float px, float py) { return PadSdf::Box(px, py, cx, cy, hx, hy, std::min(hx, hy)); },
                                0x40FFFFFF, 0xE6FFFFFF, sw);
            }
            else
            {
                const float rr = t.r * h;
                PadSdf::Outline(c, cx - rr, cy - rr, cx + rr, cy + rr, [&](float px, float py) { return PadSdf::Circle(px, py, cx, cy, rr); }, 0x40FFFFFF, 0xE6FFFFFF, sw);
            }
            if (!t.label.empty())
            {
                const int px = std::max(7, (int)(std::min(t.w > 0 ? t.h * h : t.r * h, h * 0.06f) * 0.9f));
                T(c, (int)cx - W(t.label, px, true, false) / 2, (int)cy + px * 7 / 20, t.label, px, 0xFFFFFFFF, true, false);
            }
        }
        break;
    }
    default: Whole(c, Frame(c), x, y, w, h, r); break;
    }
}
} // namespace SettingsArt

inline int Menu::FirstRow() const
{
    for (size_t i = 0; i < items.size(); i++)
        if (items[i].kind != K_SECTION && items[i].kind != K_BACK) return (int)i;
    return 0;
}

inline int Menu::UpdateSettings(const MenuInput& in, int& dir)
{
    const int n = (int)items.size();
    if (selected < 0 || selected >= n) selected = FirstRow();
    auto step = [&](int d) {
        int i = selected;
        for (int k = 0; k < n; k++)
        {
            i = (i + d + n) % n;
            if (items[i].kind != K_SECTION) { selected = i; return; }
        }
    };
    if (slotGrid && (in.up || in.down || in.left || in.right))
    {
        // (The slot grid: the pills a row, the cards rows of slotCols; Back above the pills.)
        std::vector<int> pills, cards;
        int back = -1;
        for (int i = 0; i < n; i++)
            if (items[i].kind == K_SLOT) cards.push_back(i);
            else if (items[i].kind == K_BACK) back = i;
            else if (items[i].kind != K_SECTION) pills.push_back(i);
        const int cols = std::max(1, slotCols);
        const int pc = (int)(std::find(cards.begin(), cards.end(), selected) - cards.begin());
        const int pp = (int)(std::find(pills.begin(), pills.end(), selected) - pills.begin());
        if (pc < (int)cards.size())
        {
            if (in.left && pc % cols > 0) selected = cards[pc - 1];
            if (in.right && pc % cols < cols - 1 && pc + 1 < (int)cards.size()) selected = cards[pc + 1];
            if (in.down && pc + cols < (int)cards.size()) selected = cards[pc + cols];
            if (in.up) selected = pc >= cols ? cards[pc - cols] : !pills.empty() ? pills[std::min(pc, (int)pills.size() - 1)] : back >= 0 ? back : selected;
        }
        else if (pp < (int)pills.size())
        {
            if (in.left && pp > 0) selected = pills[pp - 1];
            if (in.right && pp + 1 < (int)pills.size()) selected = pills[pp + 1];
            if (in.down && !cards.empty()) selected = cards[std::min(pp, (int)cards.size() - 1)];
            if (in.up && back >= 0) selected = back;
        }
        else if (in.down) selected = !pills.empty() ? pills[0] : !cards.empty() ? cards[0] : selected;
        return -1;
    }
    if (keyGrid && (in.up || in.down || in.left || in.right))
    {
        // (The keyboard: left and right along a row, round from one end to the other; up and down
        // to the key over or under the middle of this one; Back above the top row.)
        int back = -1, first = -1;
        for (int i = 0; i < n; i++)
            if (items[i].kind == K_BACK) back = i;
            else if (items[i].kind == K_KEY && first < 0) first = i;
        const MenuItem& s = items[selected];
        if (s.kind != K_KEY) { if (in.down && first >= 0) selected = first; return -1; }
        if (in.left || in.right)
        {
            std::vector<int> row;
            for (int i = 0; i < n; i++) if (items[i].kind == K_KEY && items[i].keyRow == s.keyRow) row.push_back(i);
            const int at = (int)(std::find(row.begin(), row.end(), selected) - row.begin()), m = (int)row.size();
            if (m > 0) selected = row[(at + (in.left ? m - 1 : 1)) % m];
            return -1;
        }
        const int r = s.keyRow + (in.down ? 1 : -1), mid = s.keyCol * 2 + s.keySpan;
        if (r < 0) { if (back >= 0) selected = back; return -1; }
        int best = -1, bestD = 1 << 30;
        for (int i = 0; i < n; i++)
            if (items[i].kind == K_KEY && items[i].keyRow == r)
            {
                const int a = items[i].keyCol * 2, b = a + items[i].keySpan * 2;
                const int d = mid >= a && mid < b ? -1 : std::min(std::abs(mid - a), std::abs(mid - b)); // (one right under it first)
                if (d < bestD) { bestD = d; best = i; }
            }
        if (best >= 0) selected = best;
        return -1;
    }
    if (in.up) step(-1);
    if (in.down) step(1);
    const MenuItem& it = items[selected];
    if (it.disabled || it.kind == K_SECTION) return -1;
    if (in.accept) return it.kind == K_PLAIN ? -1 : selected; // (A turns a toggle over and steps a picker on, as before #210; a link or Back opens)
    if (in.left || in.right)
    {
        const int d = in.left ? -1 : 1;
        // (Off is the left pill and On the right: left turns it off, right on, each only once.)
        if (it.kind == K_TOGGLE) { if ((d < 0) == (it.choice == 0)) return -1; dir = d; return selected; }
        // (Only rows with a choice to step through answer left and right: a key's row, a link or a
        // plain row waits for A, so pushing a stick sideways while moving down the list sets nothing.)
        if (it.kind == K_PICKER) { dir = d; return selected; }
    }
    return -1;
}

inline int Menu::SettingsItemAt(int x, int y) const
{
    if (drawnW <= 0 || x < drawnX || x >= drawnX + drawnW || y < drawnY || y >= drawnY + drawnH) return -2;
    if (x >= backX && x < backX + backS && y >= backY && y < backY + backS)
        for (size_t i = 0; i < items.size(); i++)
            if (items[i].kind == K_BACK) return (int)i;
    if (keyGrid)
    {
        for (const auto& r : keyRects)
            if (x >= r[0] && x < r[0] + r[2] && y >= r[1] && y < r[1] + r[3]) return r[4];
        return -1;
    }
    if (slotGrid)
    {
        for (const auto& r : slotRects)
            if (x >= r[0] && x < r[0] + r[2] && y >= r[1] && y < r[1] + r[3]) return r[4];
        return -1;
    }
    if (x < rowX || x >= rowX + rowW) return -1;
    for (size_t i = 0; i < items.size() && i < rowY.size(); i++)
        if (rowY[i] >= 0 && items[i].kind != K_SECTION && y >= rowY[i] && y < rowY[i] + rowH) return (int)i;
    return -1;
}

inline void Menu::SettingsTap(int item, int x, MenuInput& input)
{
    if (item == -2) { input.back = true; return; }
    // (A card of the preview tapped: that choice, for the chosen row.)
    if (item < 0 && selected >= 0 && selected < (int)items.size())
        for (const auto& r : pickRects)
            if (x >= r[0] && x < r[0] + r[2] && pickY >= r[1] && pickY < r[1] + r[3])
            {
                if (r[4] != items[selected].choice) pickWanted = r[4];
                return;
            }
    if (item < 0) return;
    selected = item;
    const MenuItem& it = items[item];
    if (it.disabled) return;
    if (it.kind == K_BACK) { input.back = true; return; }
    // (The slot grid and the keyboard: whatever is tapped is chosen.)
    if (slotGrid || keyGrid) { input.accept = true; return; }
    // (The visible controls: Off/On pills, chevrons, preview cards, links. The row's name only
    // focuses it; the pad reaches the same changes with Left/Right on the focused row.)
    for (const UiInput::Part& p : parts)
        if (p.item == item && UiInput::InPart(p, x, pickY))
        {
            switch (p.intent)
            {
            case UiInput::Intent::Focus: return;
            case UiInput::Intent::Step:
                if (p.value < 0) input.left = true; else input.right = true;
                return;
            case UiInput::Intent::Set:
                // (An Off/On pill: 0 is Off, 1 is On. A value of 0 must go left, not right.)
                if (p.value == 0) input.left = true; else input.right = true;
                return;
            case UiInput::Intent::Open: input.accept = true; return;
            case UiInput::Intent::Back: input.back = true; return;
            case UiInput::Intent::Drag: return;
            }
            return;
        }
}

// A type's colour (the games' own, as fans know them), for its chip and a move's edge.
inline uint32_t TypeColour(int type)
{
    static const uint32_t c[] = { 0xFFA8A77A, 0xFFC22E28, 0xFFA98FF3, 0xFFA33EA1, 0xFFE2BF65, 0xFFB6A136, 0xFFA6B91A, 0xFF735797,
        0xFFB7B7CE, 0xFF68A090, 0xFFEE8130, 0xFF6390F0, 0xFF7AC74C, 0xFFF7D02C, 0xFFF95587, 0xFF96D9D6, 0xFF6F35FC, 0xFF705746 };
    return type >= 0 && type < 18 ? c[type] : 0xFF8A93A0;
}
// An icon from the ROM (ARGB, colour 0 clear) as large as size, each pixel a block.
inline void MonIcon(Canvas& c, const std::vector<uint32_t>& icon, int iw, int ih, int x, int y, int size, int alpha = 255, uint32_t tint = 0)
{
    if (icon.empty() || iw <= 0 || ih <= 0) return;
    for (int yy = 0; yy < size; yy++)
        for (int xx = 0; xx < size; xx++)
        {
            const int X = x + xx, Y = y + yy;
            if (X < 0 || Y < 0 || X >= c.w || Y >= c.h) continue;
            const uint32_t p = icon[(size_t)(yy * ih / size) * iw + xx * iw / size];
            if (p >> 24) c.px[(size_t)Y * c.w + X] = Glass::Over(c.px[(size_t)Y * c.w + X], tint ? tint : p | 0xFF000000u, alpha); // (tint: a silhouette)
        }
    Glass::Mark(c, x, y, x + size, y + size);
}
// A type as a small coloured pill with its name.
inline int TypeChip(Canvas& c, int type, int x, int y, float k)
{
    using namespace SettingsArt;
    auto K = [&](float v) { return (int)std::lround(v * k); };
    const std::string name = CleanFont::Title(UiLang::Tr(CompanionTypes::Name(type)));
    const int px = K(12), w = W(name, px, true, false) + K(18), h = K(22);
    Glass::Round(c, x, y, w, h, h * 0.5f, TypeColour(type));
    T(c, x + K(9), y + h / 2 + px * 7 / 20, name, px, 0xFFFFFFFF, true, false);
    return w;
}

// PARTY's summary of one Pokemon (P_CMON): its icon, name, level, types, nature, item and ability;
// its stats as bars, IVs beside them; its moves, each edged in its type's colour, with PP.
inline void MonSummary(Canvas& c, const Menu::CompanionView::Mon& m, int x0, int top, int w0, int bottom, float k, bool showName = true)
{
    using namespace SettingsArt;
    auto K = [&](float v) { return (int)std::lround(v * k); };
    const uint32_t GlassText = Menu::GlassText, GlassHint = Menu::GlassHint;
    int y = top;
    const int is = K(64);
    Glass::Round(c, x0, y, is, is, 14 * k, 0x400A111C);
    Glass::Rim(c, x0, y, is, is, 14 * k, std::max(1.0f, k), 0x60, 0x14);
    if (!m.icon.empty()) MonIcon(c, m.icon, m.iconW, m.iconH, x0, y, is);
    else Glass::DrawIcon(c, Glass::I_POKEBALL, x0 + is * 0.5f, y + is * 0.5f, is * 0.28f, GlassText);
    const int tx = x0 + is + K(14);
    const std::string lv = m.egg ? UiLang::Tr("Egg") : "Lv " + std::to_string(m.level);
    if (showName)
    {
        T(c, tx, y + K(24), m.name, K(22), GlassText, true, false);
        T(c, x0 + w0 - W(lv, K(15), true, false), y + K(24), lv, K(15), GlassHint, true, false);
    }
    else T(c, tx, y + K(24), lv + (m.egg ? "" : "  \xC2\xB7  " + std::to_string(m.hp) + " / " + std::to_string(m.maxHp) + " HP"), K(16), GlassText, true, false);
    int cx = tx;
    if (CompanionTypes::Valid(m.type1)) cx += TypeChip(c, m.type1, cx, y + K(34), k) + K(6);
    if (CompanionTypes::Valid(m.type2) && m.type2 != m.type1) TypeChip(c, m.type2, cx, y + K(34), k);
    y += is + K(14);
    std::string line;
    if (!m.nature.empty()) line = UiLang::F("{0} nature", { m.nature });
    if (!m.ability.empty()) line += (line.empty() ? "" : "  \xC2\xB7  ") + m.ability;
    if (!line.empty()) { T(c, x0, y + K(12), line, K(13), Desc, false, false); y += K(20); }
    T(c, x0, y + K(12), UiLang::F("Holding {0}", { m.item.empty() ? UiLang::Tr("nothing") : m.item }), K(13), Desc, false, false);
    y += K(26);
    if (m.egg) return;
    // (Stats: each a bar, the longest the highest of the six; its IV after.)
    static const char* const names[] = { "HP", "Attack", "Defence", "Speed", "Sp. Atk", "Sp. Def" };
    static const int order[] = { 0, 1, 2, 4, 5, 3 }; // (the games' summary order)
    int best = 1;
    for (int i = 0; i < 6; i++) best = std::max(best, m.stats[i]);
    const int rowH = std::min(K(22), (bottom - y - K(140)) / 6), labelW = K(64), numW = K(40), ivW = K(44);
    if (rowH >= K(12))
    {
        for (int n = 0; n < 6; n++)
        {
            const int i = order[n];
            T(c, x0, y + rowH / 2 + K(4), names[i], K(12), GlassHint, false, false);
            const std::string v = std::to_string(m.stats[i]);
            T(c, x0 + labelW + numW - K(8) - W(v, K(13), true, false), y + rowH / 2 + K(4), v, K(13), GlassText, true, false);
            const int bx = x0 + labelW + numW, bw = w0 - labelW - numW - ivW, bh = std::max(3, K(6)), by = y + (rowH - bh) / 2;
            Glass::Round(c, bx, by, bw, bh, bh * 0.5f, 0x22FFFFFF);
            Glass::Round(c, bx, by, std::max(bh, bw * m.stats[i] / best), bh, bh * 0.5f, i == 0 ? 0xFF5BD47Au : 0xE6FFFFFFu);
            const std::string iv = "IV " + std::to_string(m.iv[i]);
            T(c, x0 + w0 - W(iv, K(11), false, false), y + rowH / 2 + K(4), iv, K(11), m.iv[i] >= 30 ? 0xFFF2C94Cu : GlassHint, false, false);
            y += rowH;
        }
        y += K(10);
    }
    // (Moves: two across, each a card edged in its type's colour, its PP under its name.)
    const int gap = K(8), mw = (w0 - gap) / 2, mh = std::min(K(50), (bottom - y - gap) / 2);
    if (mh < K(28)) return;
    for (size_t i = 0; i < m.moves.size() && i < 4; i++)
    {
        const auto& mv = m.moves[i];
        const int mx = x0 + (int)(i % 2) * (mw + gap), my = y + (int)(i / 2) * (mh + gap);
        Glass::Round(c, mx, my, mw, mh, 10 * k, Row);
        Glass::Rim(c, mx, my, mw, mh, 10 * k, std::max(1.0f, k), 0x50, 0x14);
        Glass::Round(c, mx + K(6), my + K(8), K(4), mh - K(16), K(2), TypeColour(mv.type));
        std::string name = mv.name;
        while (name.size() > 1 && W(name, K(14), true, false) > mw - K(26)) name.pop_back();
        T(c, mx + K(18), my + mh / 2 - K(2), name, K(14), GlassText, true, false);
        const std::string pp = "PP " + std::to_string(mv.pp) + (mv.maxPp > 0 ? " / " + std::to_string(mv.maxPp) : "");
        T(c, mx + K(18), my + mh / 2 + K(14), pp, K(11), mv.pp == 0 ? 0xFFE5534Bu : GlassHint, false, false);
    }
}

// BAG's summary of one item (P_CITEM): its name and pocket, what the game says of it, how many are
// in the bag, and which of the party hold one (their icons).
inline void ItemSummary(Canvas& c, const Menu::CompanionView& cv, const Menu::CompanionView::BagItem& item, int x0, int top, int w0, int bottom, float k)
{
    using namespace SettingsArt;
    auto K = [&](float v) { return (int)std::lround(v * k); };
    const uint32_t GlassText = Menu::GlassText, GlassHint = Menu::GlassHint;
    int y = top;
    const int is = K(64);
    Glass::Round(c, x0, y, is, is, 14 * k, 0x400A111C);
    Glass::Rim(c, x0, y, is, is, 14 * k, std::max(1.0f, k), 0x60, 0x14);
    if (!item.icon.empty()) MonIcon(c, item.icon, 32, 32, x0, y, is);
    else Glass::DrawIcon(c, Glass::I_LOAD, x0 + is * 0.5f, y + is * 0.5f, is * 0.26f, GlassText);
    // (Beside it how many, large, and the pocket; the name is the preview's title.)
    const int tx = x0 + is + K(16);
    const std::string q = std::to_string(item.quantity);
    T(c, tx, y + K(36), q, K(40), GlassText, true, false);
    T(c, tx + W(q, K(40), true, false) + K(10), y + K(36), "in your bag", K(19), Desc, false, false);
    T(c, tx, y + K(60), item.pocket, K(15), GlassHint, false, false);
    y += is + K(26);
    for (const auto& line : Wrap(item.desc, K(19), w0, 6)) { T(c, x0, y + K(18), line, K(19), Desc, false, false); y += K(28); }
    y += K(18);
    // (Held by: each one's icon and name, on a row.)
    if (!item.heldBy.empty() && bottom - y > K(70))
    {
        T(c, x0, y + K(14), "Held by", K(15), GlassHint, true, false);
        y += K(26);
        int cx = x0;
        for (int i : item.heldBy)
        {
            if (i < 0 || i >= (int)cv.party.size()) continue;
            const auto& m = cv.party[i];
            const int cw = K(44) + W(m.name, K(16), true, false) + K(16), ch = K(44);
            if (cx + cw > x0 + w0) break;
            Glass::Round(c, cx, y, cw, ch, ch * 0.5f, Row);
            Glass::Rim(c, cx, y, cw, ch, ch * 0.5f, std::max(1.0f, k), 0x50, 0x14);
            MonIcon(c, m.icon, m.iconW, m.iconH, cx + K(4), y + K(2), K(40));
            T(c, cx + K(44), y + ch / 2 + K(6), m.name, K(16), GlassText, true, false);
            cx += cw + K(8);
        }
        y += K(56);
    }
    if (bottom - y > K(28)) T(c, x0, bottom - K(6), "View only: use items through the game.", K(15), GlassHint, false, false);
}

// POKEDEX's summary of one entry (P_CDEXMON): its icon, number, types and whether seen or caught;
// what the Pokedex says; base stats, two columns of bars; abilities; how it evolves; the moves it
// learns by level, as chips edged in their type's colour, while they fit. One not yet seen and
// not chosen: its silhouette and how to show it.
inline void DexSummary(Canvas& c, const Menu::CompanionView::DexEntry& e, int x0, int top, int w0, int bottom, float k, bool moves = false)
{
    using namespace SettingsArt;
    auto K = [&](float v) { return (int)std::lround(v * k); };
    const uint32_t GlassText = Menu::GlassText, GlassHint = Menu::GlassHint;
    int y = top;
    // (Its battle picture from the ROM when there is one (its own page), else its party icon.)
    const bool pic = !e.hidden && !e.pic.empty() && e.picW > 0;
    const int is = pic ? K(96) : K(64);
    Glass::Round(c, x0, y, is, is, 14 * k, 0x400A111C);
    Glass::Rim(c, x0, y, is, is, 14 * k, std::max(1.0f, k), 0x60, 0x14);
    if (pic) MonIcon(c, e.pic, e.picW, e.picH, x0, y, is);
    else if (!e.icon.empty()) MonIcon(c, e.icon, 32, 32, x0, y, is, e.hidden ? 0xC0 : e.seen ? 255 : 0x50, e.hidden ? 0xFF05080Du : 0);
    const int tx = x0 + is + K(16);
    char num[16];
    snprintf(num, sizeof num, "No. %03d", e.id);
    T(c, tx, y + K(22), num, K(17), GlassText, true, false);
    // (Caught: a white pill; seen: a glass one; neither: a hint.)
    const std::string state = !e.stateKnown ? "" : e.caught ? "Caught" : e.seen ? "Seen" : "Not seen yet";
    const int sw = W(state, K(14), true, false) + K(20), sh = K(24), sx = x0 + w0 - sw;
    if (e.seen && e.stateKnown) { Glass::Round(c, sx, y + K(4), sw, sh, sh * 0.5f, e.caught ? Pill : Row); if (!e.caught) Glass::Rim(c, sx, y + K(4), sw, sh, sh * 0.5f, std::max(1.0f, k), 0x60, 0x14); }
    T(c, sx + K(10), y + K(4) + sh / 2 + K(5), state, K(14), e.caught ? PillInk : e.seen ? GlassText : GlassHint, true, false);
    if (e.hidden)
    {
        y += is + K(30);
        for (const auto& line : Wrap("You have not seen this Pokemon yet. Press Enter, or tap it, to show its entry anyway.", K(18), w0, 4))
        { T(c, x0, y + K(16), line, K(18), Desc, false, false); y += K(27); }
        return;
    }
    int cx = tx;
    if (CompanionTypes::Valid(e.type1)) cx += TypeChip(c, e.type1, cx, y + K(36), k * 1.15f) + K(6);
    if (CompanionTypes::Valid(e.type2) && e.type2 != e.type1) cx += TypeChip(c, e.type2, cx, y + K(36), k * 1.15f);
    // (The shiny one, smaller, at the right under seen or caught, where the type chips leave room.)
    const int shinyRoom = x0 + w0 - cx - K(10);
    if (pic && !e.shinyPic.empty() && shinyRoom >= K(40))
    {
        const int ss = std::min(K(60), shinyRoom), sx = x0 + w0 - ss, sy = y + K(32);
        MonIcon(c, e.shinyPic, e.picW, e.picH, sx, sy, ss);
        T(c, sx + (ss - W("Shiny", K(12), false, false)) / 2, sy + ss + K(8), "Shiny", K(12), GlassHint, false, false);
    }
    y += is + K(18);
    if (moves)
    {
        // (The moves it learns by level: two columns down the preview, each edged in its type's
        // colour, the level before it; how many more if they do not all fit.)
        T(c, x0, y + K(14), "Learns by level", K(15), GlassHint, true, false);
        y += K(24);
        const int foot = K(30), gap = K(14), colW = (w0 - gap) / 2, rowH = K(32);
        const int perCol = std::max(1, (bottom - foot - y) / rowH);
        const size_t fit = std::min(e.learns.size(), (size_t)perCol * 2);
        const int rows = (int)std::min<size_t>(perCol, (fit + 1) / 2);
        for (size_t i = 0; i < fit; i++)
        {
            const auto& l = e.learns[i];
            const int mx = x0 + (int)(i / rows) * (colW + gap), my = y + (int)(i % rows) * rowH;
            if (i / rows > 1) break;
            Glass::Round(c, mx, my + K(2), colW, rowH - K(4), 8 * k, Row);
            Glass::Round(c, mx + K(6), my + K(9), K(4), rowH - K(18), K(2), TypeColour(l.type));
            const std::string lv = std::to_string(std::max(1, l.level)); // (the level; 1: knows it from the start)
            T(c, mx + K(16), my + rowH / 2 + K(5), lv, K(14), GlassHint, false, false);
            std::string name = l.move;
            const int nx = mx + K(16) + K(26);
            while (name.size() > 1 && W(name, K(14), true, false) > colW - (nx - mx) - K(4)) name.pop_back();
            T(c, nx, my + rowH / 2 + K(5), name, K(14), GlassText, true, false);
        }
        if (fit < e.learns.size())
            T(c, x0, y + rows * rowH + K(18), UiLang::F("+{0} more", { std::to_string(e.learns.size() - fit) }), K(14), GlassHint, true, false);
        if (e.learns.empty()) T(c, x0, y + K(16), "None", K(16), Desc, false, false);
        T(c, x0, bottom - K(6), "A or tap: back to the entry", K(15), GlassHint, false, false);
        return;
    }
    for (const auto& line : Wrap(e.desc, K(17), w0, 3)) { T(c, x0, y + K(16), line, K(17), Desc, false, false); y += K(25); }
    y += K(10);
    // (Base stats: two columns of three, each a bar out of 160, the total in the heading.)
    static const char* const names[] = { "HP", "Attack", "Defence", "Speed", "Sp. Atk", "Sp. Def" };
    static const int order[] = { 0, 1, 2, 4, 5, 3 };
    int total = 0;
    for (int i = 0; i < 6; i++) total += e.base[i];
    if (total > 0 && bottom - y > K(110))
    {
        T(c, x0, y + K(14), "Base stats", K(15), GlassHint, true, false);
        const std::string t = UiLang::F("Total {0}", { std::to_string(total) });
        T(c, x0 + w0 - W(t, K(15), true, false), y + K(14), t, K(15), GlassText, true, false);
        y += K(24);
        const int gap = K(18), colW = (w0 - gap) / 2, rowH = K(26), labelW = K(62), numW = K(34);
        for (int n = 0; n < 6; n++)
        {
            const int i = order[n], col = n / 3, bx0 = x0 + col * (colW + gap), ry = y + (n % 3) * rowH;
            T(c, bx0, ry + rowH / 2 + K(6), names[i], K(15), GlassHint, false, false);
            const std::string v = std::to_string(e.base[i]);
            T(c, bx0 + labelW + numW - K(8) - W(v, K(16), true, false), ry + rowH / 2 + K(6), v, K(16), GlassText, true, false);
            const int bx = bx0 + labelW + numW, bw = colW - labelW - numW, bh = std::max(3, K(6)), by = ry + (rowH - bh) / 2;
            Glass::Round(c, bx, by, bw, bh, bh * 0.5f, 0x22FFFFFF);
            const uint32_t colr = e.base[i] >= 100 ? 0xFF5BD47Au : e.base[i] >= 60 ? 0xE6FFFFFFu : 0xFFF2C94Cu;
            Glass::Round(c, bx, by, std::max(bh, bw * std::min(e.base[i], 160) / 160), bh, bh * 0.5f, colr);
        }
        y += 3 * rowH + K(12);
    }
    // (Abilities and evolutions, a line each while they fit.)
    std::vector<std::string> lines;
    if (!e.abilities.empty())
    {
        std::string a;
        for (const auto& ab : e.abilities) a += (a.empty() ? "" : "  \xC2\xB7  ") + ab;
        lines.push_back(UiLang::F(e.abilities.size() > 1 ? "Abilities: {0}" : "Ability: {0}", { a }));
    }
    for (const auto& ev : e.evolutions) lines.push_back(ev);
    for (const auto& line : lines)
    {
        if (y + K(26) > bottom) break;
        std::string l = line;
        while (l.size() > 1 && W(l, K(16), false, false) > w0) l.pop_back();
        T(c, x0, y + K(16), l, K(16), Desc, false, false);
        y += K(25);
    }
    if (bottom - y > K(30) && !e.learns.empty()) // (the GBA games' cards have no moves to switch to)
        T(c, x0, bottom - K(6), "A or tap: the moves it learns", K(15), GlassHint, false, false);
}

// BATTLE INFO: how strong a hit is (CompanionTypes' percent) as "2x", and its colour: green for
// strong, amber for weak, red for none, white for even; "?" when not known.
inline std::string MatchText(int pct)
{
    if (pct < 0) return "?";
    if (pct == 0) return "0x";
    if (pct == 25) return "0.25x";
    if (pct == 50) return "0.5x";
    return std::to_string(pct / 100) + "x";
}
inline uint32_t MatchColour(int pct)
{
    return pct < 0 ? Menu::GlassHint : pct == 0 ? 0xFFE5534Bu : pct < 100 ? 0xFFF2C94Cu : pct > 100 ? 0xFF5BD47Au : Menu::GlassText;
}
inline const char* MatchWords(int pct)
{
    return pct < 0 ? "Not known" : pct == 0 ? "No effect" : pct < 100 ? "Not very effective" : pct > 100 ? "Super effective" : "Normal damage";
}
// BATTLE INFO's foe (P_CBFOE): its icon, level and HP; types; ability and item; then the types that
// hit it hard, that it resists and that do nothing, as chips (from its types alone).
inline void BattleFoeSummary(Canvas& c, const Menu::CompanionView::Mon& m, int x0, int top, int w0, int bottom, float k)
{
    using namespace SettingsArt;
    auto K = [&](float v) { return (int)std::lround(v * k); };
    const uint32_t GlassText = Menu::GlassText, GlassHint = Menu::GlassHint;
    int y = top;
    const int is = K(64);
    Glass::Round(c, x0, y, is, is, 14 * k, 0x400A111C);
    Glass::Rim(c, x0, y, is, is, 14 * k, std::max(1.0f, k), 0x60, 0x14);
    if (!m.icon.empty()) MonIcon(c, m.icon, m.iconW, m.iconH, x0, y, is);
    const int tx = x0 + is + K(16), tw = x0 + w0 - tx;
    const std::string lv = "Lv " + std::to_string(m.level);
    T(c, tx, y + K(20), lv, K(17), GlassText, true, false);
    const std::string hp = m.maxHp ? std::to_string(m.hp) + " / " + std::to_string(m.maxHp) + " HP" : std::string();
    if (!hp.empty()) T(c, x0 + w0 - W(hp, K(15), false, false), y + K(20), hp, K(15), Desc, false, false);
    const int bh = std::max(4, K(8)), by = y + K(32);
    const float f = m.maxHp > 0 ? std::clamp((float)m.hp / m.maxHp, 0.0f, 1.0f) : 0.0f;
    Glass::Round(c, tx, by, tw, bh, bh * 0.5f, 0x26FFFFFF);
    if (f > 0) Glass::Round(c, tx, by, std::max(bh, (int)(tw * f)), bh, bh * 0.5f, f > 0.5f ? 0xFF5BD47Au : f > 0.2f ? 0xFFF2C94Cu : 0xFFE5534Bu);
    int cx = tx;
    if (CompanionTypes::Valid(m.type1)) cx += TypeChip(c, m.type1, cx, y + K(44), k * 1.1f) + K(6);
    if (CompanionTypes::Valid(m.type2) && m.type2 != m.type1) TypeChip(c, m.type2, cx, y + K(44), k * 1.1f);
    y += is + K(24);
    if (!m.ability.empty()) { T(c, x0, y + K(14), UiLang::F("Ability: {0}", { m.ability }), K(17), Desc, false, false); y += K(26); }
    T(c, x0, y + K(14), UiLang::F("Holding {0}", { m.item.empty() ? UiLang::Tr("nothing") : m.item }), K(17), Desc, false, false);
    y += K(36);
    if (!CompanionTypes::Valid(m.type1)) return;
    // (Each group: a heading, then the types as chips with how strong, wrapping while they fit.)
    struct Group { const char* name; bool (*in)(int); };
    const Group groups[] = { { "Weak to", [](int p) { return p > 100; } }, { "Resists", [](int p) { return p > 0 && p < 100; } }, { "No effect", [](int p) { return p == 0; } } };
    for (const auto& g : groups)
    {
        std::vector<std::pair<int, int>> list;
        for (int a = 0; a < 18; a++)
            if (CompanionTypes::Valid(a))
            {
                const int p = CompanionTypes::Multiplier(a, m.type1, m.type2);
                if (g.in(p)) list.push_back({ a, p });
            }
        if (list.empty()) continue;
        std::stable_sort(list.begin(), list.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        if (y + K(56) > bottom) break;
        T(c, x0, y + K(14), g.name, K(15), GlassHint, true, false);
        y += K(24);
        int chx = x0;
        for (const auto& [type, p] : list)
        {
            const std::string name = CleanFont::Title(UiLang::Tr(CompanionTypes::Name(type))) + (p == 400 ? " 4x" : p == 25 ? " 0.25x" : "");
            const int px = K(13), cw = W(name, px, true, false) + K(18), chh = K(24);
            if (chx + cw > x0 + w0) { chx = x0; y += chh + K(6); if (y + chh > bottom) break; }
            Glass::Round(c, chx, y, cw, chh, chh * 0.5f, TypeColour(type));
            T(c, chx + K(9), y + chh / 2 + px * 7 / 20, name, px, 0xFFFFFFFF, true, false);
            chx += cw + K(6);
        }
        y += K(36);
    }
}
// BATTLE INFO's moves (P_CBMOVE): the move's type, kind, power, accuracy and PP, then how strong it
// is against the foe, large, in its colour, and anything that changes that.
inline void BattleMoveSummary(Canvas& c, const Menu::CompanionView::BattleMove& mv, const std::string& foe, int x0, int top, int w0, int bottom, float k)
{
    using namespace SettingsArt;
    auto K = [&](float v) { return (int)std::lround(v * k); };
    const uint32_t GlassText = Menu::GlassText, GlassHint = Menu::GlassHint;
    int y = top;
    int cx = x0;
    if (CompanionTypes::Valid(mv.type)) cx += TypeChip(c, mv.type, cx, y, k * 1.2f) + K(10);
    static const char* const kinds[] = { "Physical", "Special", "Status" };
    if (mv.category >= 0 && mv.category < 3) T(c, cx, y + K(19), kinds[mv.category], K(16), Desc, false, false);
    y += K(46);
    // (Power, accuracy and PP: three numbers across.)
    const std::string vals[] = { mv.power > 1 ? std::to_string(mv.power) : std::string("-"), mv.accuracy > 0 && mv.accuracy <= 100 ? std::to_string(mv.accuracy) : std::string("-"),
                                 std::to_string(mv.pp) + (mv.maxPp ? " / " + std::to_string(mv.maxPp) : "") };
    static const char* const heads[] = { "Power", "Accuracy", "PP" };
    const int colW = w0 / 3;
    for (int i = 0; i < 3; i++)
    {
        T(c, x0 + i * colW, y + K(14), heads[i], K(14), GlassHint, true, false);
        T(c, x0 + i * colW, y + K(46), vals[i], K(26), i == 2 && mv.pp == 0 ? 0xFFE5534Bu : GlassText, true, false);
    }
    y += K(76);
    T(c, x0, y + K(14), UiLang::F("Against {0}", { foe }), K(15), GlassHint, true, false);
    y += K(26);
    const std::string big = mv.category == 2 ? std::string("Status move") : MatchText(mv.percent);
    T(c, x0, y + K(38), big, K(40), mv.category == 2 ? GlassText : MatchColour(mv.percent), true, false);
    if (mv.category != 2) T(c, x0 + W(big, K(40), true, false) + K(14), y + K(38), MatchWords(mv.percent), K(18), Desc, false, false);
    y += K(58);
    if (mv.pp == 0) { T(c, x0, y + K(14), "No PP left.", K(17), 0xFFE5534Bu, false, false); y += K(26); }
    if (!mv.note.empty() && mv.category != 2)
        for (const auto& line : Wrap(mv.note, K(17), w0, 3)) { if (y + K(24) > bottom) break; T(c, x0, y + K(14), line, K(17), Desc, false, false); y += K(25); }
}
// BATTLE INFO's party (P_CBCOVER): one of your party against the foe: its best move's strength and
// the foe's worst known move's against it, two large numbers with what they mean.
inline void CoverageSummary(Canvas& c, const Menu::CompanionView::Mon& m, const Menu::CompanionView::Coverage& cov, const std::string& foe, int x0, int top, int w0, int bottom, float k)
{
    using namespace SettingsArt;
    auto K = [&](float v) { return (int)std::lround(v * k); };
    const uint32_t GlassText = Menu::GlassText, GlassHint = Menu::GlassHint;
    int y = top;
    const int is = K(64);
    Glass::Round(c, x0, y, is, is, 14 * k, 0x400A111C);
    Glass::Rim(c, x0, y, is, is, 14 * k, std::max(1.0f, k), 0x60, 0x14);
    if (!m.icon.empty()) MonIcon(c, m.icon, m.iconW, m.iconH, x0, y, is, m.hp == 0 ? 0x70 : 255);
    const int tx = x0 + is + K(16);
    T(c, tx, y + K(24), "Lv " + std::to_string(m.level) + "  \xC2\xB7  " + (m.hp ? std::to_string(m.hp) + " / " + std::to_string(m.maxHp) + " HP" : UiLang::Tr("Fainted")), K(16), GlassText, true, false);
    int cx = tx;
    if (CompanionTypes::Valid(m.type1)) cx += TypeChip(c, m.type1, cx, y + K(36), k * 1.1f) + K(6);
    if (CompanionTypes::Valid(m.type2) && m.type2 != m.type1) TypeChip(c, m.type2, cx, y + K(36), k * 1.1f);
    y += is + K(26);
    const struct { const char* head; int pct; std::string say; } rows[] = {
        { "Its best move", cov.out, UiLang::F("against {0}", { foe }) },
        { "The foe's worst known move", cov.in, "against it" } };
    for (const auto& r : rows)
    {
        if (y + K(80) > bottom) break;
        T(c, x0, y + K(14), r.head, K(15), GlassHint, true, false);
        const std::string big = MatchText(r.pct);
        T(c, x0, y + K(56), big, K(40), MatchColour(r.pct), true, false);
        T(c, x0 + W(big, K(40), true, false) + K(14), y + K(56), std::string(MatchWords(r.pct)) + " " + r.say, K(17), Desc, false, false);
        y += K(84);
    }
}

// COMPANION's pictures of what the game holds now (Menu::CompanionView), in the box x0..x0+w0,
// top..bottom: a settings page's preview (DrawSettings), the Companion Window and the HUD.
inline bool CompanionPicture(Canvas& c, const Menu::CompanionView& cv, int kind, int x0, int top, int w0, int bottom, float k,
                             int chosen = -1, std::vector<std::array<int, 5>>* picks = nullptr, const std::string& title = std::string())
{
    using namespace SettingsArt;
    const uint32_t GlassText = Menu::GlassText, GlassHint = Menu::GlassHint;
    auto K = [&](float v) { return (int)std::lround(v * k); };
    if (kind == Menu::P_CPARTY && bottom - top > K(80))
    {
        // (PARTY: a card for each Pokemon: its icon, name, level, HP and held item.)
        const auto& party = cv.party;
        // (One under another: each name and its HP have the whole width.)
        const int cols = 1, rows = 6, gap = K(8);
        const int cw = w0, ch = std::min(K(62), (bottom - top - (rows - 1) * gap) / rows);
        const int y0 = top + std::max(0, (bottom - top - rows * ch - (rows - 1) * gap) / 2);
        for (int i = 0; i < 6; i++)
        {
            const int cx = x0 + (i % cols) * (cw + gap), cy = y0 + (i / cols) * (ch + gap);
            if (i >= (int)party.size())
            {
                Glass::Round(c, cx, cy, cw, ch, 12 * k, 0x0CFFFFFF, 0x1AFFFFFF, std::max(1.0f, k)); // (an empty place)
                continue;
            }
            const auto& m = party[i];
            Glass::Round(c, cx, cy, cw, ch, 12 * k, Row);
            Glass::Rim(c, cx, cy, cw, ch, 12 * k, std::max(1.0f, k), 0x60, 0x14);
            const int is = std::min(ch - K(8), K(56)), ix = cx + K(8), iy = cy + (ch - is) / 2;
            if (!m.icon.empty() && m.iconW > 0 && m.iconH > 0)
            {
                // (The icon, as large as the space, each of its pixels a block: kept sharp.)
                const int dw = is, dh = is * m.iconH / m.iconW, dx = ix, dy = iy + (is - dh) / 2;
                for (int yy = 0; yy < dh; yy++)
                    for (int xx = 0; xx < dw; xx++)
                    {
                        const int X = dx + xx, Y = dy + yy;
                        if (X < 0 || Y < 0 || X >= c.w || Y >= c.h) continue;
                        const uint32_t px = m.icon[(size_t)(yy * m.iconH / dh) * m.iconW + xx * m.iconW / dw];
                        if (px >> 24) c.px[(size_t)Y * c.w + X] = Glass::Over(c.px[(size_t)Y * c.w + X], px | 0xFF000000u, (int)(px >> 24));
                    }
                Glass::Mark(c, dx, dy, dx + dw, dy + dh);
            }
            else Glass::DrawIcon(c, Glass::I_POKEBALL, ix + is * 0.5f, iy + is * 0.5f, is * 0.3f, m.egg ? GlassHint : GlassText);
            // (Name and level above, the HP bar across under them with its numbers at its end.)
            const int tx = ix + is + K(10), tw = cx + cw - K(12) - tx;
            const int nameY = cy + ch / 2 - K(4), barY = cy + ch / 2 + K(6);
            std::string name = m.name;
            const std::string lv = m.egg ? UiLang::Tr("Egg") : "Lv " + std::to_string(m.level);
            while (name.size() > 1 && W(name, K(16), true, false) > tw - W(lv, K(13), false, false) - K(16)) name.pop_back();
            T(c, tx, nameY, name, K(16), GlassText, true, false);
            T(c, cx + cw - K(12) - W(lv, K(13), false, false), nameY, lv, K(13), GlassHint, false, false);
            if (!m.egg)
            {
                // (HP: green over half, yellow over a fifth, red below.)
                const std::string hp = m.hp == 0 ? "Fainted" : std::to_string(m.hp) + " / " + std::to_string(m.maxHp);
                const int hw = W(hp, K(12), false, false) + K(10), bh = std::max(3, K(6)), bw = tw - hw;
                const float f = m.maxHp > 0 ? std::clamp((float)m.hp / m.maxHp, 0.0f, 1.0f) : 0.0f;
                const uint32_t col = f > 0.5f ? 0xFF5BD47Au : f > 0.2f ? 0xFFF2C94Cu : 0xFFE5534Bu;
                Glass::Round(c, tx, barY, bw, bh, bh * 0.5f, 0x26FFFFFF);
                if (f > 0) Glass::Round(c, tx, barY, std::max(bh, (int)(bw * f)), bh, bh * 0.5f, col);
                T(c, tx + bw + K(10), barY + bh / 2 + K(4), hp, K(12), m.hp == 0 ? 0xFFE5534Bu : Desc, false, false);
            }
        }
    }
    else if (kind == Menu::P_CBAG && bottom - top > K(60))
    {
        // (BAG: each pocket a card with how many kinds of thing it holds and how many in all.)
        const auto& pk = cv.pockets;
        const int n = (int)pk.size(), cols = 2, rows = std::max(1, (n + cols - 1) / cols), gap = K(10);
        const int cw = (w0 - gap) / cols, ch = std::min(K(64), (bottom - top - (rows - 1) * gap) / rows);
        const int y0 = top + std::max(0, (bottom - top - rows * ch - (rows - 1) * gap) / 2);
        for (int i = 0; i < n; i++)
        {
            const int cx = x0 + (i % cols) * (cw + gap), cy = y0 + (i / cols) * (ch + gap);
            const bool empty = pk[i].stacks == 0, on = i == chosen;
            // (BAG's POCKET row: the pocket shown lit as a choice card, each card tapped to show it.)
            Glass::Round(c, cx, cy, cw, ch, 12 * k, on ? Pill : empty ? 0x0CFFFFFF : Row);
            if (!empty || on) Glass::Rim(c, cx, cy, cw, ch, 12 * k, std::max(1.0f, k), on ? 0x90 : 0x60, 0x14);
            if (picks) picks->push_back({ cx, cy, cw, ch, i });
            T(c, cx + K(14), cy + ch / 2 - K(2), pk[i].name, K(15), on ? PillInk : empty ? GlassHint : GlassText, true, false);
            const std::string sub = empty ? "Empty" : UiLang::F(pk[i].stacks == 1 ? "{0} kind" : "{0} kinds", { std::to_string(pk[i].stacks) }) + "  \xC2\xB7  " + UiLang::F("{0} in all", { std::to_string(pk[i].total) });
            T(c, cx + K(14), cy + ch / 2 + K(16), sub, K(12), on ? 0xB30B1018u : GlassHint, false, false);
        }
    }
    else if (kind == Menu::P_CDEX && bottom - top > K(80))
    {
        // (POKEDEX: seen and caught, each a large number and a bar of the whole Pokedex.)
        const int total = std::max(1, cv.dexTotal);
        const int bh = K(10), gap = K(46), blockH = 2 * (K(40) + K(14) + bh) + gap;
        int y = top + std::max(0, (bottom - top - blockH) / 2);
        if (picks)
        {
            // (POKEDEX's SHOW row: All, Seen and Caught as cards, the one shown lit, each tapped
            // to show it; the seen and caught bars under them.)
            const int g = K(10), cw = (w0 - 2 * g) / 3, ch = K(70);
            const struct { const char* name; int n; } cards[] = { { "All", cv.dexTotal }, { "Seen", cv.seen }, { "Caught", cv.caught } };
            for (int i = 0; i < 3; i++)
            {
                const int cx = x0 + i * (cw + g), on = i == chosen;
                Glass::Round(c, cx, top, cw, ch, 12 * k, on ? Pill : Row);
                Glass::Rim(c, cx, top, cw, ch, 12 * k, std::max(1.0f, k), on ? 0x90 : 0x60, 0x14);
                picks->push_back({ cx, top, cw, ch, i });
                T(c, cx + K(14), top + K(30), cards[i].name, K(16), on ? PillInk : GlassText, true, false);
                T(c, cx + K(14), top + K(54), std::to_string(cards[i].n), K(15), on ? 0xB30B1018u : GlassHint, false, false);
            }
            y = std::max(top + ch + K(30), y);
        }
        const struct { const char* name; int n; } lines[] = { { "Seen", cv.seen }, { "Caught", cv.caught } };
        for (const auto& l : lines)
        {
            const std::string num = std::to_string(l.n);
            T(c, x0, y + K(36), num, K(38), GlassText, true, false);
            T(c, x0 + W(num, K(38), true, false) + K(12), y + K(36), UiLang::F("{0} of {1}", { std::string(l.name), std::to_string(total) }), K(16), Desc, false, false);
            const int pct = (int)std::lround(100.0 * l.n / total);
            const std::string ps = std::to_string(pct) + "%";
            T(c, x0 + w0 - W(ps, K(16), true, false), y + K(36), ps, K(16), GlassText, true, false);
            const int by = y + K(40) + K(14);
            Glass::Round(c, x0, by, w0, bh, bh * 0.5f, 0x26FFFFFF);
            if (l.n > 0) Glass::Round(c, x0, by, std::max(bh, w0 * l.n / total), bh, bh * 0.5f, Pill);
            y = by + bh + gap;
        }
    }
    else if (kind == Menu::P_CPLACE && bottom - top > K(60))
    {
        // (LOCATION: the place's name, then the region's map from the ROM with the player on it.)
        int y = top;
        // (Not again when the preview's title already is the place's name.)
        if (!cv.place.empty() && cv.place != title) { T(c, x0, y + K(26), cv.place, K(24), GlassText, true, false); y += K(38); }
        if (!cv.area.empty()) { T(c, x0, y + K(10), cv.area, K(14), GlassHint, false, false); y += K(22); }
        if (!cv.map.empty() && cv.mapW > 0 && cv.mapH > 0 && bottom - y > K(60))
        {
            int mx, my, mw, mh;
            Fit((float)cv.mapW / cv.mapH, x0, y + K(8), w0, bottom - y - K(8), mx, my, mw, mh);
            Glass::Round(c, mx - K(4), my - K(4), mw + K(8), mh + K(8), 12 * k, 0x1A000000, 0x48FFFFFF, std::max(1.0f, 1.2f * k));
            for (int yy = 0; yy < mh; yy++)
                for (int xx = 0; xx < mw; xx++)
                {
                    const int X = mx + xx, Y = my + yy;
                    if (X < 0 || Y < 0 || X >= c.w || Y >= c.h) continue;
                    c.px[(size_t)Y * c.w + X] = cv.map[(size_t)(yy * cv.mapH / mh) * cv.mapW + xx * cv.mapW / mw] | 0xFF000000u;
                }
            Glass::Mark(c, mx, my, mx + mw, my + mh);
            if (cv.markerX >= 0 && cv.markerY >= 0)
            {
                const float px = mx + cv.markerX * mw, py = my + cv.markerY * mh, r = 7 * k;
                Glass::Round(c, (int)(px - r * 2), (int)(py - r * 2), (int)(r * 4), (int)(r * 4), r * 2, 0x40FFFFFF);
                Glass::Round(c, (int)(px - r), (int)(py - r), (int)(r * 2), (int)(r * 2), r, 0xFFE5534Bu, 0xFFFFFFFF, std::max(1.5f, 2 * k));
            }
        }
    }
    else if (kind == Menu::P_CBATTLE && bottom - top > K(80))
    {
        // (BATTLE: the foe's card and yours, as the party's, then how the types match up.)
        int y = top;
        for (size_t i = 0; i < cv.battle.size() && i < 2; i++)
        {
            const auto& m = cv.battle[i];
            const int ch = K(62);
            T(c, x0, y + K(14), i == 0 ? "Foe" : "Yours", K(13), GlassHint, true, false);
            y += K(20);
            Glass::Round(c, x0, y, w0, ch, 12 * k, Row);
            Glass::Rim(c, x0, y, w0, ch, 12 * k, std::max(1.0f, k), 0x60, 0x14);
            const int is = ch - K(8), ix = x0 + K(8), iy = y + K(4);
            if (!m.icon.empty() && m.iconW > 0)
            {
                for (int yy = 0; yy < is; yy++)
                    for (int xx = 0; xx < is; xx++)
                    {
                        const int X = ix + xx, Y = iy + yy;
                        if (X < 0 || Y < 0 || X >= c.w || Y >= c.h) continue;
                        const uint32_t p = m.icon[(size_t)(yy * m.iconH / is) * m.iconW + xx * m.iconW / is];
                        if (p >> 24) c.px[(size_t)Y * c.w + X] = p | 0xFF000000u;
                    }
                Glass::Mark(c, ix, iy, ix + is, iy + is);
            }
            const int tx = ix + is + K(10), tw = x0 + w0 - K(12) - tx;
            const std::string lv = "Lv " + std::to_string(m.level);
            T(c, tx, y + ch / 2 - K(4), m.name, K(16), GlassText, true, false);
            T(c, x0 + w0 - K(12) - W(lv, K(13), false, false), y + ch / 2 - K(4), lv, K(13), GlassHint, false, false);
            const int bh = std::max(3, K(6)), by = y + ch / 2 + K(6);
            const float f = m.maxHp > 0 ? std::clamp((float)m.hp / m.maxHp, 0.0f, 1.0f) : 0.0f;
            const uint32_t col = f > 0.5f ? 0xFF5BD47Au : f > 0.2f ? 0xFFF2C94Cu : 0xFFE5534Bu;
            Glass::Round(c, tx, by, tw, bh, bh * 0.5f, 0x26FFFFFF);
            if (f > 0) Glass::Round(c, tx, by, std::max(bh, (int)(tw * f)), bh, bh * 0.5f, col);
            if (!m.item.empty()) T(c, tx, by + bh + K(14), m.item, K(12), Desc, false, false); // (its types)
            y += ch + K(8);
        }
        y += K(8);
        for (const std::string& line : cv.battleLines)
        {
            for (const std::string& l : Wrap(CleanFont::Title(line), K(14), w0, 2))
            {
                if (y + K(20) > bottom) break;
                T(c, x0, y + K(14), l, K(14), Desc, false, false);
                y += K(20);
            }
        }
    }
    else return false;
    return true;
}

inline void Menu::DrawSettings(Canvas& c, const std::string& title, const std::string& hint) const
{
    double profT = MenuProfNow();
    using namespace SettingsArt;
    // (The frosted panel over the game is the same on every settings page of a screen: drawn once
    // while the menu is open and copied after, as drawing it each time cost the Switch most of a
    // press.)
    static std::vector<uint32_t> frame;
    static std::string frameKey;
    const std::string fk = std::to_string(c.w) + "x" + std::to_string(c.h) + " " + std::to_string(c.BackdropSig()) + " " + std::to_string(UiScale());
    const bool haveFrame = fk == frameKey && frame.size() == c.px.size();
    const bool solid = haveFrame || laneX >= 0 ? c.backdrop.size() == c.px.size() : Glass::Backdrop(c); // (in the launcher: its page is drawn already)
    const int n = (int)items.size();
    pickRects.clear();
    parts.clear();
    if (selected < 0 || selected >= n) return;
    const bool wide = c.w >= c.h * 1.2f;
    float pw, ph, k;
    if (wide && UiLarge())
    {
        // (Larger menus, on a small screen: the panel fills it, as there is no room to spare
        // around it, and its contents are larger by as much; the rows scroll.)
        pw = c.w - 2.0f * std::max(2, c.h / 120);
        ph = c.h - 2.0f * std::max(2, c.h / 120);
        k = std::min(ph / 645.0f, pw / 1045.0f) * UiScale();
    }
    else if (wide)
    {
        pw = std::min(c.w * 0.76f, c.h * 1.62f);
        ph = std::min(c.h * 0.84f, pw * 0.62f);
        k = ph / 645.0f * UiScale();
    }
    else
    {
        pw = c.w * 0.94f;
        ph = c.h * 0.9f;
        k = std::min(pw / 560.0f, ph / 1000.0f) * UiScale();
    }
    k = std::max(k, 0.42f);
    const bool lane = laneX >= 0;
    if (lane) k = laneK > 0 ? laneK : std::max(0.42f, c.h / 720.0f * 0.94f * UiScale());
    auto K = [&](float v) { return (int)std::lround(v * k); };
    int px0 = (int)((c.w - pw) / 2), py0 = (int)((c.h - ph) / 2), W0 = (int)pw, H0 = (int)ph;
    if (lane) { px0 = laneX; py0 = laneY; W0 = laneW; H0 = laneH; }
    drawnX = px0; drawnY = py0; drawnW = W0; drawnH = H0; drawnLineH = 0; drawnFirstY = 0;

    // The panel.
    if (lane) {}
    else if (haveFrame)
    {
        std::copy(frame.begin(), frame.end(), c.px.begin());
        Glass::Mark(c, 0, 0, c.w, c.h);
    }
    else
    {
        Glass::Shadow(c, px0, py0, W0, H0, 28 * k, 26 * k, 0x80);
        if (solid) Glass::Frost(c, px0, py0, W0, H0, 28 * k);
        else Glass::Round(c, px0, py0, W0, H0, 28 * k, GlassShade); // (no blurred game to frost: the old shade)
        Glass::Round(c, px0, py0, W0, H0, 28 * k, Panel);
        Glass::Rim(c, px0, py0, W0, H0, 28 * k, std::max(1.2f, 1.8f * k));
        frame = c.px;
        frameKey = fk;
    }

    { const double t = MenuProfNow(); MenuProf()[0] += t - profT; profT = t; }
    // Header: back button, the page's icon, its title, a line under them.
    backS = lane ? 0 : K(54); backX = px0 + K(28); backY = py0 + K(18);
    if (backS > 0)
        for (int i = 0; i < n; i++)
            if (items[i].kind == K_BACK)
            {
                parts.push_back({backX, backY, backS, backS, UiInput::Intent::Back, i, 0});
                break;
            }
    if (!lane)
    {
    const bool backChosen = items[selected].kind == K_BACK;
    if (backChosen) GlowRound(c, backX, backY, backS, backS, 14 * k, k);
    Glass::Round(c, backX, backY, backS, backS, 14 * k, backChosen ? RowChosen : Row, backChosen ? ChosenEdge : PanelEdge, backChosen ? 2 * k : std::max(1.0f, k));
    Glass::DrawIcon(c, Glass::I_BACK, backX + backS * 0.5f, backY + backS * 0.5f, 13 * k, GlassText);
    const int iconX = backX + backS + K(18), iconS = K(58), iconY = py0 + K(16);
    Glass::Round(c, iconX, iconY, iconS, iconS, 15 * k, Row, PanelEdge, std::max(1.0f, k));
    if (pageIcon >= 0) Glass::DrawIcon(c, pageIcon, iconX + iconS * 0.5f, iconY + iconS * 0.5f, 17 * k, GlassText);
    T(c, iconX + iconS + K(20), iconY + K(41), title, K(34), GlassText, true);
    if (!headerNote.empty()) T(c, px0 + W0 - K(32) - W(headerNote, K(17)), iconY + K(37), headerNote, K(17), GlassHint);
    FadeLine(c, px0 + K(28), px0 + W0 - K(28), py0 + K(92), std::max(1, K(1.5f)), 0xFFFFFF, 0x90);
    }

    // Where the rows and the preview go.
    const int top = lane ? py0 + K(14) : py0 + K(108), bottom = lane ? py0 + H0 - K(14) : py0 + H0 - K(24);
    if (slotGrid) { DrawSlotGrid(c, px0 + K(lane ? 14 : 30), W0 - K(lane ? 28 : 60), top, bottom, k); return; }
    if (keyGrid) { DrawKeyGrid(c, px0 + K(lane ? 14 : 30), W0 - K(lane ? 28 : 60), top, bottom, k); return; }
    int listX, listW, listBottom, pvX, pvY, pvW, pvH;
    if (wide)
    {
        listX = px0 + K(30);
        listW = (int)(W0 * listShare);
        listBottom = bottom;
        pvX = listX + listW + K(30);
        pvY = top + K(4);
        pvW = px0 + W0 - K(22) - pvX;
        pvH = bottom - pvY;
        if (!lane) Rule(c, listX + listW + K(18), top + K(8), std::max(1, K(1)), bottom - top - K(16), Line);
    }
    else
    {
        listX = px0 + K(24);
        listW = W0 - K(48);
        listBottom = top + (int)((bottom - top) * 0.56f);
        pvX = listX;
        pvY = listBottom + K(16);
        pvW = listW;
        pvH = bottom - pvY;
    }
    if (lane)
    {
        listX = px0 + K(14); listW = laneListW - K(28); listBottom = bottom;
        pvX = lanePvX; pvY = py0; pvW = px0 + W0 - pvX; pvH = H0;
        if (laneListH > 0) { listBottom = py0 + laneListH - K(10); pvY = lanePvY; pvH = py0 + H0 - lanePvY; }
    }
    previewX = pvX; previewY = pvY; previewW = pvW; previewH = pvH; // (for a finger's drag on the Guide's answer)

    { const double t = MenuProfNow(); MenuProf()[1] += t - profT; profT = t; }
    // The rows: their places down the list, a section's heading (and a line before every one but
    // the first) taking a little more room than a row.
    // (Closer together, down to nine tenths, before the list has to scroll.)
    // (Phones: the rows larger than the rest of the page, to suit a finger: touchRows.)
    const float kr = k * touchRows;
    auto KR = [&](float v) { return (int)std::lround(v * kr); };
    const int avail = listBottom - top;
    std::vector<int> at(n, -1);
    int total = 0, pitch = 0, headH = 0, sepH = 0;
    for (float squeeze : { 1.0f, 0.95f, 0.9f })
    {
        rowH = KR(40 * squeeze * rowTall); pitch = rowH + KR(4 * squeeze); headH = KR(46 * squeeze); sepH = KR(14 * squeeze);
        total = 0;
        bool firstSection = true;
        for (int i = 0; i < n; i++)
        {
            if (items[i].kind == K_BACK) continue;
            if (items[i].kind == K_SECTION)
            {
                if (!firstSection) total += sepH;
                firstSection = false;
                at[i] = total;
                total += headH;
            }
            else { at[i] = total; total += pitch; }
        }
        if (total <= avail) break;
    }
    if (tallRows)
    {
        // (A few links, as SETTINGS: the rows share the list's height, up to 130 high each.)
        int rows = 0;
        for (int i = 0; i < n; i++) if (items[i].kind != K_BACK && items[i].kind != K_SECTION) rows++;
        if (rows > 0)
        {
            pitch = std::min(KR(130), avail / rows);
            rowH = pitch - KR(12);
            total = 0;
            for (int i = 0; i < n; i++)
                if (items[i].kind != K_BACK && items[i].kind != K_SECTION) { at[i] = total; total += pitch; }
        }
    }
    rowX = listX; rowW = listW;
    if (total <= avail) scrollPx = 0;
    else if (freeScroll) scrollPx = std::clamp(scrollPx, 0, total - avail);
    else if (at[selected] >= 0)
    {
        // (The chosen row kept in view; its section's heading too when that fits.)
        int y0 = at[selected];
        for (int i = selected - 1; i >= 0; i--)
            if (items[i].kind == K_SECTION) { if (at[selected] - at[i] <= avail - pitch) y0 = at[i]; break; }
            else if (items[i].kind != K_BACK) break;
        if (y0 < scrollPx) scrollPx = y0;
        if (at[selected] + rowH > scrollPx + avail) scrollPx = at[selected] + rowH - avail;
        scrollPx = std::clamp(scrollPx, 0, total - avail);
    }
    rowY.assign(n, -1);
    ctlW = KR(186);
    if (c.h > c.w) ctlW = std::max(KR(110), (int)(listW * 0.33f)); // (upright: the value under a third of the row, so the name has room)
    else if (UiLarge()) ctlW = std::min(ctlW, std::max(KR(120), (int)(listW * 0.42f))); // (larger menus: the name keeps its room on a short list)
    ctlX = listX + listW - KR(14) - ctlW;
    bool firstSection = true;
    for (int i = 0; i < n; i++)
    {
        const MenuItem& it = items[i];
        if (at[i] < 0) continue;
        const int y = top + at[i] - scrollPx;
        const bool isSection = it.kind == K_SECTION;
        if (isSection && !firstSection && y - sepH / 2 >= top && y - sepH / 2 < listBottom)
            Rule(c, listX, y - sepH / 2 - 1, listW, std::max(1, KR(1)), Line);
        if (isSection) firstSection = false;
        if (y < top || y + (isSection ? headH : rowH) > listBottom) continue; // (scrolled out of view)
        rowY[i] = y;
        if (isSection)
        {
            if (it.icon >= 0) Glass::DrawIcon(c, it.icon, listX + KR(22), y + headH * 0.52f, 12 * kr, GlassText);
            T(c, listX + KR(50), y + headH * 2 / 3 + KR(1), it.label, KR(22), GlassText, true);
            continue;
        }
        {
            // (The row as the pointer sees it: its name focuses only; links open; Off/On pills and
            // the chevrons are the controls. The pad reaches the same changes with Left/Right.
            // These are registered before the row's pixels: a row copied from the cache still
            // needs its parts, or its controls answer only once it has been drawn again.)
            // (A row that opens something is a button across its whole width: the label, the value
            // and the chevron all open it. A value row's name only focuses; its controls change it.)
            const bool opens = it.kind == K_LINK || it.kind == K_FOLDER || it.kind == K_SLOT || it.kind == K_KEY || it.kind == K_ACTION;
            parts.push_back({listX, y, opens ? listW : std::max(0, ctlX - listX), rowH,
                             it.kind == K_BACK ? UiInput::Intent::Back : opens ? UiInput::Intent::Open : UiInput::Intent::Focus, i, 0});
            if (it.kind == K_TOGGLE)
            {
                parts.push_back({ctlX, y, ctlW / 2, rowH, UiInput::Intent::Set, i, 0});
                parts.push_back({ctlX + ctlW / 2, y, ctlW - ctlW / 2, rowH, UiInput::Intent::Set, i, 1});
            }
            else if (it.kind == K_PICKER)
            {
                parts.push_back({ctlX, y, ctlW / 2, rowH, UiInput::Intent::Step, i, -1});
                parts.push_back({ctlX + ctlW / 2, y, ctlW - ctlW / 2, rowH, UiInput::Intent::Step, i, 1});
            }
        }
        // (Each row is drawn once and copied after, while it stays the same: its words, its place, and
        // whether it or the row above is the chosen one (that one's glow shows through it).
        // Drawing every row each press cost the Switch about 60 ms.)
        const bool selHere = i == selected;
        const bool nearSel = i > 0 && i - 1 == selected; // (the row above's glow, drawn before this one)
        std::string rk = std::to_string(Row) + "|" + it.label + "|" + it.sub + "|" + it.value + "|" + std::to_string(it.choice) + "|" + std::to_string(it.kind) + "|" +
            std::to_string(it.preview) + "|" + std::to_string(it.icon) + "|" + std::to_string(it.indent) + "|" + std::to_string(it.disabled) + "|" +
            std::to_string(it.slider) + "|" + std::to_string(listX) + "," + std::to_string(y) + "," + std::to_string(listW) + "," + std::to_string(rowH) + "|" +
            std::to_string(nearSel) + std::to_string(tallRows) + std::to_string(rowTall) + std::to_string(kr) + "|" + std::to_string(c.BackdropSig()) + "|" +
            std::to_string(companion.party.size()) + std::to_string(companion.bag.size()) + std::to_string(companion.dex.size()) + std::to_string(companion.moves.size()) +
            "|" + std::to_string(it.thumb.size()) + "," + std::to_string(it.thumb.empty() ? 0u : it.thumb[it.thumb.size() / 2]); // (a badge come since)
        for (const auto& ch : it.choices) rk += "|" + ch;
        struct RowPixels { std::string key; int x = 0, y = 0, w = 0, h = 0; std::vector<uint32_t> px; };
        static std::vector<RowPixels> rowCache;
        if (rowCache.size() < (size_t)n) rowCache.resize(n);
        RowPixels& rc = rowCache[i];
        const int cx0 = std::max(0, listX), cy0 = std::max(0, y), cx1 = std::min(c.w, listX + listW), cy1 = std::min(c.h, y + rowH);
        if (!selHere && rc.key == rk && rc.w == cx1 - cx0 && rc.h == cy1 - cy0 && rc.w > 0 && rc.h > 0)
        {
            for (int yy = 0; yy < rc.h; yy++) std::copy(&rc.px[(size_t)yy * rc.w], &rc.px[(size_t)yy * rc.w] + rc.w, &c.px[(size_t)(cy0 + yy) * c.w + cx0]);
            Glass::Mark(c, cx0, cy0, cx1, cy1);
            continue;
        }
        auto drawRow = [&]() {
            const bool sel = i == selected, off = it.disabled;
            if (sel) GlowRound(c, listX, y, listW, rowH, 10 * kr, kr);
            Glass::Round(c, listX, y, listW, rowH, 10 * kr, sel ? RowChosen : Row, sel ? ChosenEdge : 0, sel ? 2.2f * kr : std::max(1.0f, k));
            if (!sel) Glass::Rim(c, listX, y, listW, rowH, 10 * kr, std::max(1.0f, k), 0x50, 0x14);
            const uint32_t ink = off ? GlassOff : GlassText;
            if (tallRows)
            {
                // (A tall row: its icon in a glass circle, the name large, a line under it, what it is
                // set to now on the right before the arrow.)
                const int d = std::min((int)(rowH * 0.62f), KR(64)), cxI = listX + KR(22) + d / 2, cyI = y + rowH / 2;
                Glass::Round(c, cxI - d / 2, cyI - d / 2, d, d, d * 0.5f, 0x400A111C);
                Glass::Rim(c, cxI - d / 2, cyI - d / 2, d, d, d * 0.5f, std::max(1.0f, kr), 0x60, 0x14);
                if (it.preview == P_CMON && it.choice >= 0 && it.choice < (int)companion.party.size() && !companion.party[it.choice].icon.empty())
                {
                    const auto& pm = companion.party[it.choice];
                    MonIcon(c, pm.icon, pm.iconW, pm.iconH, cxI - d / 2, cyI - d / 2, d, pm.hp == 0 && !pm.egg ? 0x70 : 255);
                }
                else if (it.icon >= 0) Glass::DrawIcon(c, it.icon, (float)cxI, (float)cyI, d * 0.27f, ink);
                const int tx = cxI + d / 2 + KR(22);
                const int vpx2 = std::min(KR(18), (int)(rowH * 0.2f));
                const int vw = it.value.empty() ? 0 : W(it.value, vpx2) + KR(20);
                const int maxW = listX + listW - KR(50) - vw - tx;
                const bool twoLines = !it.sub.empty() && rowH >= KR(54);
                // (Sizes from the row's height, so they sit the same on a PC and a phone.)
                const int lpx = std::min(KR(26), (int)(rowH * 0.28f)), spx = std::min(KR(17), (int)(rowH * 0.18f));
                T(c, tx, twoLines ? y + (int)(rowH * 0.47f) : y + rowH / 2 + lpx * 7 / 20, it.label, lpx, ink, true);
                if (twoLines)
                {
                    std::string sub = UiLang::Tr(it.sub);
                    if (W(sub, spx, false, false) > maxW)
                    {
                        while (sub.size() > 1 && W(sub + "...", spx, false, false) > maxW) { sub.pop_back(); while (!sub.empty() && (sub.back() & 0xC0) == 0x80) sub.pop_back(); }
                        sub += "...";
                    }
                    T(c, tx, y + (int)(rowH * 0.77f), sub, spx, Desc, false, false);
                }
                Glass::DrawIcon(c, Glass::I_CHEVRON_R, listX + listW - KR(28), y + rowH * 0.5f, 10 * kr, ink);
                if (!it.value.empty()) T(c, listX + listW - KR(48) - W(it.value, vpx2), y + rowH / 2 + vpx2 * 7 / 20, it.value, vpx2, GlassHint);
                return;
            }
            const int in = it.indent * KR(26); // (a row inside an open folder)
            if (it.kind == K_FOLDER) Glass::DrawIcon(c, it.choice == 1 ? Glass::I_CHEVRON_D : Glass::I_CHEVRON_R, listX + KR(36), y + rowH * 0.5f, 9 * kr, ink);
            else if (!it.noArt && it.preview == P_CITEM && it.choice >= 0 && it.choice < (int)companion.bag.size() && !companion.bag[it.choice].icon.empty())
            {
                const int d = std::min(rowH - KR(4), KR(36 * rowTall)); // (BAG: the item's own icon from the ROM)
                MonIcon(c, companion.bag[it.choice].icon, 32, 32, listX + KR(36) + in - d / 2, y + (rowH - d) / 2, d);
            }
            else if (!it.noArt && it.preview == P_CDEXMON && it.choice >= 0 && it.choice < (int)companion.dex.size() && !companion.dex[it.choice].icon.empty())
            {
                const auto& e = companion.dex[it.choice]; // (POKEDEX: its icon, faint until seen)
                const int d = std::min(rowH, KR(40 * rowTall));
                MonIcon(c, e.icon, 32, 32, listX + KR(36) + in - d / 2, y + (rowH - d) / 2, d, e.hidden ? 0xC0 : e.seen ? 255 : 0x50, e.hidden ? 0xFF05080Du : 0);
            }
            else if ((it.preview == P_CBFOE || it.preview == P_CBCOVER) && it.choice >= -1)
            {
                // (BATTLE INFO: the foe's icon, or that of one of your party)
                const Menu::CompanionView::Mon* pm = it.preview == P_CBFOE ? &companion.foe
                    : it.choice >= 0 && it.choice < (int)companion.coverage.size() && companion.coverage[it.choice].party >= 0 &&
                      companion.coverage[it.choice].party < (int)companion.party.size() ? &companion.party[companion.coverage[it.choice].party] : nullptr;
                const int d = std::min(rowH, KR(40 * rowTall));
                if (pm && !pm->icon.empty()) MonIcon(c, pm->icon, pm->iconW, pm->iconH, listX + KR(36) + in - d / 2, y + (rowH - d) / 2, d, pm->hp == 0 ? 0x70 : 255);
            }
            else if (it.preview == P_CBMOVE && it.choice >= 0 && it.choice < (int)companion.moves.size())
            {
                const int bw = KR(8), bh = (int)(rowH * 0.5f); // (BATTLE INFO: the move's type as a bar of its colour)
                Glass::Round(c, listX + KR(36) + in - bw / 2, y + (rowH - bh) / 2, bw, bh, bw * 0.5f, TypeColour(companion.moves[it.choice].type));
            }
            else if (!it.noArt && !it.thumb.empty() && it.thumbW > 0 && it.thumbH > 0)
            {
                // (A picture of its own, as an achievement's badge: in place of the icon, rounded.)
                const int d = std::min(rowH - KR(8), KR(34 * rowTall));
                const int bx = listX + KR(36) + in - d / 2, by = y + (rowH - d) / 2;
                MonIcon(c, it.thumb, it.thumbW, it.thumbH, bx, by, d);
                Glass::Rim(c, bx, by, d, d, KR(6), std::max(1.0f, kr), 0x50, 0x14);
            }
            else if (it.icon >= 0) Glass::DrawIcon(c, it.icon, listX + KR(36) + in, y + rowH * 0.5f, 11 * kr, ink);
            const int lpx = KR(rowTall > 1 || UiLarge() ? 22 : 20); // (the name's size; larger menus: as a tall row's, the controls being narrower)
            const int base = y + rowH / 2 + lpx * 7 / 20; // (the baseline of the row's text)
            {
                // (A long name cut to fit before the controls on the right.)
                int maxW = (it.kind == K_TOGGLE || it.kind == K_PICKER ? ctlX : listX + listW - KR(90)) - (listX + KR(76) + in) - KR(12);
                // (A row that opens or does something with a value on its right ("8 ADDED"): the name ends
                // before it. Translated, the two ran into each other: "Iconos En el Menú Home" over "8 Añadidos".)
                if (it.kind == K_LINK && !it.value.empty())
                    maxW = std::min(maxW, listX + listW - KR(44) - W(it.value, KR(rowTall > 1 ? 20 : 17), rowTall > 1) - KR(12) - (listX + KR(76) + in));
                std::string label = it.label;
                const bool bold = it.kind == K_FOLDER;
                int lp2 = lpx; // (A long name shrinks a little first, and is cut only if it still does not fit.)
                while (lp2 * 100 > lpx * 78 && W(label, lp2, bold) > maxW) lp2--;
                std::string line2; // (still too long: two lines, split at the last space that lets both fit)
                if (W(label, lp2, bold) > maxW && rowH >= KR(40))
                {
                    const int lp3 = lpx * 80 / 100;
                    for (size_t k = label.rfind(' '); k != std::string::npos && k > 0; k = label.rfind(' ', k - 1))
                        if (W(label.substr(0, k), lp3, bold) <= maxW && W(label.substr(k + 1), lp3, bold) <= maxW) { line2 = label.substr(k + 1); label = label.substr(0, k); lp2 = lp3; break; }
                }
                if (line2.empty() && W(label, lp2, bold) > maxW)
                {
                    while (label.size() > 1 && W(label + "...", lp2, bold) > maxW) label.pop_back();
                    label += "...";
                }
                if (line2.empty()) T(c, listX + KR(76) + in, base, label, lp2, ink, bold);
                else
                {
                    T(c, listX + KR(76) + in, base - lp2 * 3 / 5, label, lp2, ink, bold);
                    T(c, listX + KR(76) + in, base + lp2 * 3 / 5, line2, lp2, ink, bold);
                }
            }
            if (it.kind == K_FOLDER && !it.value.empty())
                T(c, listX + listW - KR(20) - W(it.value, KR(17)), base, it.value, KR(17), GlassHint);
            if (it.kind == K_TOGGLE)
            {
                const int hw = (ctlW - KR(6)) / 2, ph2 = std::min(rowH - KR(6), KR(32)), py = y + (rowH - ph2) / 2;
                for (int s = 0; s < 2; s++)
                {
                    const bool on = s == it.choice;
                    const int x = ctlX + s * (hw + KR(6));
                    Glass::Round(c, x, py, hw, ph2, ph2 * 0.5f, on ? (off ? 0x50FFFFFF : Pill) : PillOff, on ? (off ? 0x40FFFFFF : PillEdge) : PillOffEdge, std::max(1.0f, k));
                    const std::string& name = s < (int)it.choices.size() ? it.choices[s] : (s ? std::string("ON") : std::string("OFF"));
                    TCentred(c, x + hw / 2, py + ph2 / 2 + KR(6), name, KR(18), off ? GlassOff : on ? PillInk : 0xFFDCE2EA, on);
                }
            }
            else if (it.kind == K_PICKER || (it.kind == K_PLAIN && !it.value.empty()))
            {
                const bool arrows = it.kind == K_PICKER && !off;
                if (arrows)
                {
                    const bool canL = it.choice > 0, canR = it.choice + 1 < (int)it.choices.size();
                    Glass::DrawIcon(c, Glass::I_CHEVRON_L, ctlX + KR(14), y + rowH * 0.5f, 9 * kr, canL ? GlassText : GlassOff);
                    Glass::DrawIcon(c, Glass::I_CHEVRON_R, ctlX + ctlW - KR(14), y + rowH * 0.5f, 9 * kr, canR ? GlassText : GlassOff);
                }
                int vpx = KR(19);
                while (vpx > 8 && W(it.value, vpx, true) > ctlW - KR(UiLarge() ? 40 : 56)) vpx--; // (larger menus: the arrows closer, the value kept large)
                if (it.kind == K_PICKER) TCentred(c, ctlX + ctlW / 2, base, it.value, vpx, ink, true);
                else T(c, ctlX + ctlW - W(it.value, vpx), base, it.value, vpx, off ? GlassOff : GlassHint);
            }
            else if (it.kind == K_LINK)
            {
                const bool opens = it.preview != P_CITEM && it.preview != P_CDEXMON && it.preview != P_CGUIDE &&
                                   it.preview != P_CBFOE && it.preview != P_CBMOVE && it.preview != P_CBCOVER; // (BAG's items, POKEDEX's entries: all in the preview)
                if (opens) Glass::DrawIcon(c, Glass::I_CHEVRON_R, listX + listW - KR(26), y + rowH * 0.5f, 9 * kr, ink);
                const int vpx = KR(rowTall > 1 ? 20 : 17); // (BAG: how many, as large as the name)
                if (!it.value.empty()) T(c, listX + listW - KR(opens ? 44 : 24) - W(it.value, vpx, rowTall > 1), base, it.value, vpx,
                                         it.preview == P_CBMOVE && it.choice >= 0 && it.choice < (int)companion.moves.size() && companion.moves[it.choice].category != 2
                                             ? MatchColour(companion.moves[it.choice].percent) : rowTall > 1 ? GlassText : GlassHint, rowTall > 1);
            }
        };
        drawRow();
        if (!selHere && cx1 > cx0 && cy1 > cy0)
        {
            rc.key = rk; rc.x = cx0; rc.y = cy0; rc.w = cx1 - cx0; rc.h = cy1 - cy0;
            rc.px.resize((size_t)rc.w * rc.h);
            for (int yy = 0; yy < rc.h; yy++) std::copy(&c.px[(size_t)(cy0 + yy) * c.w + cx0], &c.px[(size_t)(cy0 + yy) * c.w + cx0] + rc.w, &rc.px[(size_t)yy * rc.w]);
        }
    }
    if (total > avail) // (a thin bar: where the rows in view are among them all)
    {
        const int bx = listX + listW + KR(6), bw = std::max(2, KR(3));
        Glass::Round(c, bx, top, bw, avail, bw * 0.5f, 0x18FFFFFF);
        const int th = std::max(KR(24), avail * avail / total), ty = top + (avail - th) * scrollPx / std::max(1, total - avail);
        Glass::Round(c, bx, ty, bw, th, bw * 0.5f, 0x70FFFFFF);
    }

    { const double t = MenuProfNow(); MenuProf()[2] += t - profT; profT = t; }
    // The preview of the chosen row.
    const MenuItem& it = items[selected];
    PadStyleNow() = padStyle;
    PadLabelsNow() = padLabels;
    TouchNow() = touchShapes;
    PadLiveNow() = padLive;
    std::copy(padAxes, padAxes + 6, PadAxesNow());
    KeyNow() = it.value;
    {
        // (The preview's panel, the same for every row: drawn once and copied after.)
        static std::vector<uint32_t> pvPixels;
        static std::string pvKey;
        const std::string key = std::to_string(pvX) + "," + std::to_string(pvY) + "," + std::to_string(pvW) + "," + std::to_string(pvH) + " " +
            std::to_string(k) + " " + std::to_string(c.w) + "x" + std::to_string(c.h) + " " + std::to_string(c.BackdropSig());
        const int x0 = std::max(0, pvX), y0 = std::max(0, pvY), x1 = std::min(c.w, pvX + pvW), y1 = std::min(c.h, pvY + pvH);
        if (pvKey == key && pvPixels.size() == (size_t)std::max(0, x1 - x0) * std::max(0, y1 - y0) && x1 > x0)
        {
            for (int yy = y0; yy < y1; yy++) std::copy(&pvPixels[(size_t)(yy - y0) * (x1 - x0)], &pvPixels[(size_t)(yy - y0) * (x1 - x0)] + (x1 - x0), &c.px[(size_t)yy * c.w + x0]);
            Glass::Mark(c, x0, y0, x1, y1);
        }
        else
        {
            Glass::Round(c, pvX, pvY, pvW, pvH, 18 * k, Row);
            Glass::Rim(c, pvX, pvY, pvW, pvH, 18 * k, std::max(1.0f, 1.2f * k), 0x70, 0x18);
            if (x1 > x0 && y1 > y0)
            {
                pvPixels.resize((size_t)(x1 - x0) * (y1 - y0));
                for (int yy = y0; yy < y1; yy++) std::copy(&c.px[(size_t)yy * c.w + x0], &c.px[(size_t)yy * c.w + x0] + (x1 - x0), &pvPixels[(size_t)(yy - y0) * (x1 - x0)]);
                pvKey = key;
            }
        }
    }
    const int pad = K(20), innerW = pvW - 2 * pad;
    // (A Pokedex entry's card: its name over it, whichever of its rows is chosen.)
    const bool dexCard = it.preview == P_CDEXMON && it.choice >= 0 && it.choice < (int)companion.dex.size() && !companion.dex[it.choice].hidden;
    const std::string name = it.kind == K_BACK ? std::string("BACK") : dexCard ? companion.dex[it.choice].name : it.label;
    const std::string desc = it.kind == K_BACK ? std::string("Back to the page before.") : Sentence(it.desc);
    // (A long name, as GUIDE's questions: on two lines, the rest moved down.)
    const float pf = touchRows > 0 ? 1.22f : 1.0f; // (a phone: the preview's writing bigger)
    const int nameS = (int)(K(25) * pf), descS = (int)(K(18) * pf), descL = (int)(K(26) * pf);
    int y = pvY + K(80) + (int)(K(8) * (pf - 1.0f) * 4);
    if (W(name, nameS, true) > innerW)
    {
        const auto nameLines = Wrap(name, nameS, innerW - K(30), 2);
        for (size_t i = 0; i < nameLines.size(); i++) T(c, pvX + pad, pvY + K(42) + (int)i * K(32), nameLines[i], nameS, GlassText, true);
        y += (int)(nameLines.size() - 1) * K(32);
    }
    else T(c, pvX + pad, pvY + K(42), name, nameS, GlassText, true);
    // (A cheat's button requirement: a bright card under the name, not just a line of small text.)
    if (it.preview == P_CHEATNOTE && !it.sub.empty())
    {
        const int boxH = K(58);
        Glass::Round(c, pvX + pad, y, innerW, boxH, 12 * k, 0x400A111C);
        Glass::Rim(c, pvX + pad, y, innerW, boxH, 12 * k, std::max(1.0f, k), 0x60, 0x14);
        TCentred(c, pvX + pad + innerW / 2, y + boxH / 2 + K(7), it.sub, K(21), GlassText, true);
        y += boxH + K(12);
    }
    // (The description stops above the tip at the foot of the preview: with larger menus on a short
    // screen it ran into it.)
    size_t descMax = wide ? 7 : 5;
    if (UiLarge() && !it.tip.empty())
    {
        const int tipLines = (int)Wrap(Sentence(it.tip), K(17), innerW - K(34), 4).size();
        const int tipTop = pvY + pvH - pad - tipLines * K(23);
        const int room = tipTop - K(18) - K(8) - (y - descL + K(6));
        descMax = std::min(descMax, (size_t)std::max(1, room / std::max(1, descL)));
    }
    for (const std::string& line : Wrap(desc, descS, innerW, descMax))
    {
        T(c, pvX + pad, y, line, descS, Desc, false, false);
        y += descL;
    }
    int regionTop = y + K(10), regionBottom = pvY + pvH - pad;
    if (!it.tip.empty())
    {
        const int tipPx = K(17), tipLine = K(23);
        const auto lines = Wrap(Sentence(it.tip), tipPx, innerW - K(34), 4);
        const int tipTop = pvY + pvH - pad - (int)lines.size() * tipLine;
        Rule(c, pvX + pad, tipTop - K(18), innerW, std::max(1, K(1)), Line);
        Glass::DrawIcon(c, Glass::I_INFO, pvX + pad + K(12), tipTop + K(6), 11 * k, Desc);
        for (size_t i = 0; i < lines.size(); i++) T(c, pvX + pad + K(34), tipTop + K(12) + (int)i * tipLine, lines[i], tipPx, Desc, false, false);
        regionBottom = tipTop - K(34);
    }
    { const double t = MenuProfNow(); MenuProf()[3] += t - profT; profT = t; }
    // The choices: a picture of each, the one on now edged and its name in a violet pill.
    // (More than six choices, as 3D RESOLUTION's eight: only the first, the DS's own, beside the one
    // on now.)
    const bool onlyOn = it.preview == P_RES && it.choice >= 0;
    const int choices = onlyOn ? (it.choice > 0 ? 2 : 1) : (int)it.choices.size();
    if (it.preview == P_SHORTCUTS && shortcutKeys.size() == 4 && regionBottom - regionTop > K(60))
    {
        // (CONTROLS' SHORTCUTS: Fulldex's four, each a tile with its icon, name, key and button.)
        static const int icons[] = { Glass::I_FAST, Glass::I_SCREENS, Glass::I_SAVE, Glass::I_LOAD };
        static const char* const names[] = { "FAST FORWARD", "SWAP SCREENS", "SAVE STATE", "LOAD STATE" };
        const int gap = K(12), tw = (innerW - gap) / 2, th = std::min(K(104), (regionBottom - regionTop - gap) / 2);
        const int y0 = regionTop + std::max(0, (regionBottom - regionTop - 2 * th - gap) / 2);
        for (int i = 0; i < 4; i++)
        {
            const int tx = pvX + pad + (i % 2) * (tw + gap), ty = y0 + (i / 2) * (th + gap);
            Glass::Round(c, tx, ty, tw, th, 12 * k, Row, RowEdge, std::max(1.0f, k));
            Glass::DrawIcon(c, icons[i], tx + K(22), ty + K(22), 10 * k, GlassText);
            T(c, tx + K(40), ty + K(27), names[i], K(15), GlassText, true);
            const std::string kb = shortcutKeys[i], pd = i < (int)shortcutPads.size() ? shortcutPads[i] : std::string();
            int ly = ty + K(54);
            if (!kb.empty()) { T(c, tx + K(16), ly, "KEY", K(13), GlassHint); T(c, tx + tw - K(14) - W(kb, K(15), true), ly, kb, K(15), GlassText, true); ly += K(26); }
            if (!pd.empty()) { T(c, tx + K(16), ly, "BUTTON", K(13), GlassHint); T(c, tx + tw - K(14) - W(pd, K(15), true), ly, pd, K(15), GlassText, true); }
        }
    }
    else if (it.preview == P_CMON && it.choice >= 0 && it.choice < (int)companion.party.size() && regionBottom - regionTop > K(120))
        MonSummary(c, companion.party[it.choice], pvX + pad, regionTop, innerW, regionBottom, k, false);
    else if (it.preview == P_CDEXMON && it.choice >= 0 && it.choice < (int)companion.dex.size() && regionBottom - regionTop > K(120))
        DexSummary(c, companion.dex[it.choice], pvX + pad, regionTop, innerW, regionBottom, k, companion.dexMoves);
    else if (it.preview == P_CBFOE && regionBottom - regionTop > K(120))
        BattleFoeSummary(c, companion.foe, pvX + pad, regionTop, innerW, regionBottom, k);
    else if (it.preview == P_CBMOVE && it.choice >= 0 && it.choice < (int)companion.moves.size() && regionBottom - regionTop > K(120))
        BattleMoveSummary(c, companion.moves[it.choice], companion.foe.name, pvX + pad, regionTop, innerW, regionBottom, k);
    else if (it.preview == P_CBCOVER && it.choice >= 0 && it.choice < (int)companion.coverage.size() && regionBottom - regionTop > K(120))
    {
        const auto& cov = companion.coverage[it.choice];
        if (cov.party >= 0 && cov.party < (int)companion.party.size())
            CoverageSummary(c, companion.party[cov.party], cov, companion.foe.name, pvX + pad, regionTop, innerW, regionBottom, k);
    }
    else if (it.preview == P_CGUIDE && it.choice >= 0 && it.choice < (int)companion.guide.size() && regionBottom - regionTop > K(60))
    {
        // (GUIDE: the answer under ANSWER once shown; until then a card saying how to show it.)
        const auto& a = companion.guide[it.choice];
        const int x0 = pvX + pad;
        int gy = regionTop + K(6);
        if (a.shown)
        {
            T(c, x0, gy + K(14), "Answer", K(15), GlassHint, true, false);
            gy += K(26);
            // (The answer's own line breaks first: HERE puts each Pokémon on a line, under the way
            // it is met. Each line still wraps if it is wider than the panel. A longer answer
            // scrolls with Left/Right; the bar on the right says how much there is.)
            std::vector<std::string> lines;
            size_t at = 0;
            while (at <= a.text.size())
            {
                size_t nl = a.text.find('\n', at);
                const std::string para = a.text.substr(at, nl == std::string::npos ? std::string::npos : nl - at);
                for (const auto& line : Wrap(para, K(18), innerW - K(10), 80)) lines.push_back(line);
                if (nl == std::string::npos) break;
                at = nl + 1;
            }
            const int lineH = K(24);
            previewLineH = lineH; // (a finger's drag on the answer moves it a line at a time)
            const int room = std::max(0, (regionBottom - gy) / lineH);
            const int maxScroll = std::max(0, (int)lines.size() - room);
            a.scrollMax = maxScroll; // (the guide's Left/Right clamps to it)
            const int first = std::clamp(a.scroll, 0, maxScroll);
            const int textTop = gy;
            for (int i = first; i < (int)lines.size() && i < first + room; i++)
            {
                T(c, x0, gy + K(16), lines[i], K(18), GlassText, false, false);
                gy += lineH;
            }
            if (maxScroll > 0)
            {
                const int barX = x0 + innerW - K(7);
                const int barTop = textTop + K(2);
                const int barH = std::max(K(20), regionBottom - barTop);
                Glass::Round(c, barX, barTop, K(3), barH, K(2), 0x30FFFFFF);
                const int thumbH = std::max(K(18), barH * room / (int)lines.size());
                const int thumbY = barTop + (barH - thumbH) * first / std::max(1, maxScroll);
                Glass::Round(c, barX, thumbY, K(3), thumbH, K(2), 0xA0FFFFFF);
            }
        }
        else
        {
            const int ch = std::min(K(110), regionBottom - gy);
            Glass::Round(c, x0, gy, innerW, ch, 14 * k, 0x400A111C);
            Glass::Rim(c, x0, gy, innerW, ch, 14 * k, std::max(1.0f, k), 0x60, 0x14);
            TCentred(c, x0 + innerW / 2, gy + ch / 2 - K(4), "Answer hidden", K(18), GlassText, true);
            TCentred(c, x0 + innerW / 2, gy + ch / 2 + K(22), "A or tap to show it", K(15), GlassHint, false);
        }
    }
    else if (it.preview == P_CITEM && it.choice >= 0 && it.choice < (int)companion.bag.size() && regionBottom - regionTop > K(120))
        ItemSummary(c, companion, companion.bag[it.choice], pvX + pad, regionTop, innerW, regionBottom, k);
    else if ((it.preview == P_CPARTY || it.preview == P_CBAG || it.preview == P_CDEX || it.preview == P_CPLACE) &&
             CompanionPicture(c, companion, it.preview, pvX + pad, regionTop, innerW, regionBottom, k,
                              it.kind == K_PICKER ? it.choice : -1, it.kind == K_PICKER ? &pickRects : nullptr, it.label))
    {
    }
    else if (it.preview == P_TOUCH && regionBottom - regionTop > K(60))
    {
        // (TOUCH CONTROLS: the game with the on-screen buttons where they are, screen-shaped.)
        int tx, ty, tw, th;
        Fit(std::clamp((float)c.w / c.h, 1.3f, 2.3f), pvX + pad, regionTop, innerW, regionBottom - regionTop, tx, ty, tw, th);
        Glass::Round(c, tx - K(4), ty - K(4), tw + K(8), th + K(8), 12 * k, 0x1A000000, 0x48FFFFFF, std::max(1.0f, 1.2f * k));
        Picture(c, P_TOUCH, 0, tx, ty, tw, th, k);
    }
    else if (it.preview == P_PAD && !padLabels.empty() && it.kind != K_BACK && regionBottom - regionTop > K(60))
    {
        // (KEYS AND BUTTONS: the controller as large as the space allows, what each button does
        // written beside it; the row's own button lit.)
        const int noteH = padNote.empty() ? 0 : K(26);
        Picture(c, P_PAD, it.art.empty() ? -1 : it.art[0], pvX + pad / 2, regionTop, pvW - pad, regionBottom - regionTop + K(10) - noteH, k);
        if (!padNote.empty()) TCentred(c, pvX + pvW / 2, regionBottom + K(6), padNote, K(15), Desc);
    }
    else if (it.kind != K_BACK && it.kind != K_LINK && it.preview > P_NONE && it.preview != P_CHEATNOTE && choices > 0 && regionBottom - regionTop > K(60))
    {
        // (FPS's film strips are wide: one above the other, each with its name beside it.)
        const bool side = it.preview == P_MOTION;
        const int cols = side ? 1 : choices <= 3 ? choices : choices <= 6 ? 3 : 4, rows = (choices + cols - 1) / cols;
        const int gapX = K(14), gapY = K(12), labelH = K(28), labelGap = K(10), sideW = side ? K(58) : 0;
        const int captionH = side ? K(26) : 0; // (under the strips: what the white edge means)
        const int regionW = innerW, regionH = regionBottom - regionTop - captionH;
        const int cellW = (regionW - (cols - 1) * gapX) / cols, cellH = (regionH - (rows - 1) * gapY) / rows;
        const float aspect = it.preview == P_SECOND ? 0.95f : side ? 3.2f : it.preview == P_PAD && !padLabels.empty() ? 1.25f : it.preview == P_PAD || it.preview == P_KEY ? 1.6f
                           : std::clamp((float)c.w / c.h, 1.3f, 2.1f);
        int picW = side ? std::min(cellW - sideW - gapX, (int)((cellH - K(8)) * aspect))
                        : std::min((int)(cellW * 0.94f), (int)((cellH - labelH - labelGap) * aspect));
        // (Two side by side, as wide as a third of the panel each at most: the design's size.)
        if (!side) picW = std::min(picW, (int)(innerW * (cols == 1 ? 0.96f : cols == 2 ? 0.46f : cols == 3 ? 0.32f : 0.24f)));
        const bool bare = (it.preview == P_PAD || it.preview == P_KEY) && choices == 1; // (one controller: no card round it)
        const int picH = (int)(picW / aspect);
        const bool pictures = picH >= K(26);
        const int rowH = side ? std::max(picH + K(8), labelH) : (pictures ? picH + labelGap : 0) + labelH;
        const int blockH = rows * rowH + (rows - 1) * gapY + (pictures ? captionH : 0);
        const int y0 = regionTop + std::max(0, (regionH + captionH - blockH) / 2);
        const int sideX = pvX + pad + (innerW - (sideW + gapX + picW)) / 2; // (strips and names centred)
        for (int i = 0; i < choices; i++)
        {
            const int col = i % cols, row = i / cols;
            int cx = side ? sideX + sideW / 2 : pvX + pad + col * (cellW + gapX) + cellW / 2;
            int cy = y0 + row * (rowH + gapY);
            const int ci = onlyOn ? (i == choices - 1 ? it.choice : 0) : i; // (the choice this card shows)
            const bool on = ci == it.choice;
            if (!onlyOn && !it.disabled && (it.kind == K_PICKER || it.kind == K_TOGGLE)) // (a card to tap: its whole cell)
            {
                const std::array<int, 5> r{ side ? sideX : cx - cellW / 2, cy - K(6), side ? sideW + gapX + picW : cellW, rowH + K(12), ci };
                pickRects.push_back(r);
                parts.push_back({r[0], r[1], r[2], r[3], UiInput::Intent::Set, selected, ci});
            }
            if (pictures)
            {
                const int x = side ? sideX + sideW + gapX : cx - picW / 2, py = side ? cy + (rowH - picH) / 2 : cy;
                if (on && !it.disabled && !bare) GlowRound(c, x - K(4), py - K(4), picW + K(8), picH + K(8), 12 * k, k);
                if (!bare)
                    Glass::Round(c, x - K(4), py - K(4), picW + K(8), picH + K(8), 12 * k, on ? 0x30FFFFFF : 0x1A000000,
                                 on ? ChosenEdge : 0x48FFFFFF, on ? 2.4f * k : std::max(1.0f, 1.2f * k));
                Picture(c, it.preview, ci < (int)it.art.size() ? it.art[ci] : ci, x, py, picW, picH, k);
                if (!side) cy += picH + labelGap;
            }
            if (side) cy += (rowH - labelH) / 2;
            int lpx = K(16);
            while (lpx > 8 && W(it.choices[ci], lpx, on) + K(30) > (side ? sideW + K(10) : cellW)) lpx--;
            const int lw = side ? sideW : W(it.choices[ci], lpx, on) + K(30);
            Glass::Round(c, cx - lw / 2, cy, lw, labelH, labelH * 0.5f, on ? Pill : PillOff, on ? PillEdge : PillOffEdge, std::max(1.0f, k));
            TCentred(c, cx, cy + labelH / 2 + lpx * 7 / 20, it.choices[ci], lpx, on ? PillInk : 0xFFDCE2EA, on);
        }
        if (side && pictures)
        {
            const int ly = y0 + rows * rowH + (rows - 1) * gapY + K(20);
            const int cxm = pvX + pvW / 2, lpx = K(14);
            const std::string cap = "The added frames are edged in white.";
            const int tw = W(cap, lpx, false, false) + K(22);
            Glass::Round(c, cxm - tw / 2, ly - K(11), K(12), K(12), 2 * k, 0, 0xFFFFFFFF, std::max(1.0f, 1.4f * k));
            T(c, cxm - tw / 2 + K(20), ly, cap, lpx, Desc, false, false);
        }
    }
    else if (it.slider >= 0 && regionBottom - regionTop > K(40))
    {
        // (A slider, as VOLUME: ten bars, as many lit as it is set to, and its value under them.)
        const int bars = 10, gap = K(6), bw = std::min(K(22), (innerW - 9 * gap) / bars), total = bars * bw + (bars - 1) * gap;
        const int bx = pvX + (pvW - total) / 2, bh = K(70), by = regionTop + (regionBottom - regionTop - bh - K(40)) / 2;
        for (int b = 0; b < bars; b++)
        {
            const int h = bh * (b + 3) / (bars + 2);
            Glass::Round(c, bx + b * (bw + gap), by + bh - h, bw, h, bw * 0.3f, b < it.slider ? Pill : PillOff, b < it.slider ? 0 : PillOffEdge, std::max(1.0f, k));
        }
        TCentred(c, pvX + pvW / 2, by + bh + K(32), it.value, K(22), GlassText, true);
    }
    else if (it.kind != K_SLOT && !it.thumb.empty() && it.thumbW > 0 && it.thumbH > 0 && regionBottom - regionTop > K(140))
    {
        // (A picture of its own, as an achievement's badge: large, in a glass frame, its value under it.)
        const int d = std::min(K(150), regionBottom - regionTop - K(60)), cx = pvX + pvW / 2;
        const int cy = regionTop + (regionBottom - regionTop - d - K(50)) / 2;
        Glass::Round(c, cx - d / 2 - K(8), cy - K(8), d + K(16), d + K(16), 20 * k, 0x400A111C);
        MonIcon(c, it.thumb, it.thumbW, it.thumbH, cx - d / 2, cy, d);
        Glass::Rim(c, cx - d / 2 - K(8), cy - K(8), d + K(16), d + K(16), 20 * k, std::max(1.0f, 1.4f * k), 0x80, 0x18);
        if (!it.value.empty()) TCentred(c, cx, cy + d + K(44), it.value, K(20), GlassText, true);
    }
    else if (it.kind == K_LINK && it.icon >= 0 && it.preview <= P_NONE && regionBottom - regionTop > K(120))
    {
        // (A page of its own, as on the SETTINGS page: its icon large in a glass circle, and what it
        // is set to now under it.)
        const int d = K(120), cx = pvX + pvW / 2, cy = regionTop + (regionBottom - regionTop - d - K(50)) / 2;
        Glass::Round(c, cx - d / 2, cy, d, d, d * 0.5f, 0x400A111C);
        Glass::Rim(c, cx - d / 2, cy, d, d, d * 0.5f, std::max(1.0f, 1.4f * k), 0x80, 0x18);
        Glass::DrawIcon(c, it.icon, (float)cx, cy + d * 0.5f, 30 * k, GlassText);
        if (!it.value.empty()) TCentred(c, cx, cy + d + K(40), it.value, K(20), GlassText, true);
    }
    else if (it.kind != K_BACK && !it.value.empty() && regionBottom - regionTop > K(40))
    {
        // (No pictures, as for LANGUAGE: the choice on now, large.)
        const int vpx = K(22), lw = W(it.value, vpx, true) + K(44), lh = K(42);
        const int cx = pvX + pvW / 2, cy = regionTop + (regionBottom - regionTop - lh) / 2;
        Glass::Round(c, cx - lw / 2, cy, lw, lh, lh * 0.5f, Pill, PillEdge, std::max(1.0f, k));
        TCentred(c, cx, cy + lh / 2 + vpx * 7 / 20, it.value, vpx, PillInk, true);
    }

    { const double t = MenuProfNow(); MenuProf()[4] += t - profT; profT = t; }
    // The buttons' hint under the panel, if there is room.
    if (!hint.empty() && c.h - (py0 + H0) > K(30))
        TCentred(c, c.w / 2, py0 + H0 + K(30), hint, K(15), GlassHint);
    if (solid && !haveFrame) Glass::Opaque(c); // (the copied panel is solid already, and so is all drawn over it)
    { const double t = MenuProfNow(); MenuProf()[5] += t - profT; MenuProf()[6] += 1; }
}

// style 2: a question in a raised glass card ("Quit Game?"), the paused game frosted behind it.
// The buttons: CANCEL (the last item) on the left, the action (items[0]) on the right; the chosen
// one a white pill.
// SAVE and LOAD (Menu::slotGrid): the page's other rows as pills across the top (QUICK SAVE,
// BACKUPS, THUMBNAILS), then the state slots as cards, five across (two on a portrait screen), each
// with the game's top screen as it was saved, the slot's number and when; an empty one a faint
// outline. The chosen card or pill glows as a row does.
inline void Menu::DrawSlotGrid(Canvas& c, int x0, int w0, int top, int bottom, float k) const
{
    using namespace SettingsArt;
    auto K = [&](float v) { return (int)std::lround(v * k); };
    slotRects.clear();
    const int n = (int)items.size();
    std::vector<int> pills, cards;
    for (int i = 0; i < n; i++)
        if (items[i].kind == K_SLOT) cards.push_back(i);
        else if (items[i].kind != K_BACK && items[i].kind != K_SECTION) pills.push_back(i);
    // (The pills: each its icon, name and, for a toggle, On or Off.)
    const int ph = K(52);
    int px = x0, rowTop = top;
    for (int i : pills)
    {
        const MenuItem& it = items[i];
        const bool sel = i == selected;
        const std::string state = it.kind == K_TOGGLE ? (it.choice == 1 ? "On" : "Off") : it.value;
        const int lw = W(it.label, K(18), true), sw = state.empty() ? 0 : W(state, K(16)) + K(14);
        const int pw = K(it.icon >= 0 ? 58 : 28) + lw + sw + K(26);
        if (px + pw > x0 + w0 && px > x0) { px = x0; rowTop += ph + K(12); } // (a phone: on to a second row)
        const int top = rowTop;
        if (sel) GlowRound(c, px, top, pw, ph, ph * 0.5f, k);
        Glass::Round(c, px, top, pw, ph, ph * 0.5f, sel ? RowChosen : Row, sel ? ChosenEdge : 0, sel ? 2.2f * k : std::max(1.0f, k));
        if (!sel) Glass::Rim(c, px, top, pw, ph, ph * 0.5f, std::max(1.0f, k), 0x50, 0x14);
        int tx = px + K(24);
        if (it.icon >= 0) { Glass::DrawIcon(c, it.icon, tx + K(10), top + ph * 0.5f, 10 * k, GlassText); tx += K(34); }
        T(c, tx, top + ph / 2 + K(7), it.label, K(18), GlassText, true);
        if (!state.empty()) T(c, tx + lw + K(14), top + ph / 2 + K(7), state, K(16), it.kind == K_TOGGLE && it.choice != 1 ? GlassHint : Desc);
        slotRects.push_back({ px, top, pw, ph, i });
        px += pw + K(12);
    }
    // (The cards: as large as fit, the picture 4:3, the words under it.)
    const bool wide = c.w >= c.h * 1.2f;
    const int cols = wide ? 5 : 2;
    slotCols = cols;
    const int rows = std::max(1, ((int)cards.size() + cols - 1) / cols);
    const int gy = rowTop + ph + K(24), gap = K(16);
    const int textH = K(58);
    int cw = (w0 - (cols - 1) * gap) / cols;
    int imgH = cw * 3 / 4;
    const int availH = bottom - gy;
    if (rows * (imgH + textH) + (rows - 1) * gap > availH) { imgH = std::max(K(40), (availH - (rows - 1) * gap) / rows - textH); }
    const int ch = imgH + textH;
    for (size_t q = 0; q < cards.size(); q++)
    {
        const int i = cards[q];
        const MenuItem& it = items[i];
        const bool sel = i == selected;
        const int cx = x0 + (int)(q % cols) * (cw + gap), cy = gy + (int)(q / cols) * (ch + gap);
        if (cy + ch > bottom + K(4)) break;
        const bool empty = it.value.empty();
        // (Each card is drawn once and copied after while it stays the same, as the rows of other
        // pages: the chosen one, and one beside or under it (its glow), drawn each time.)
        const int sq = (int)(std::find(cards.begin(), cards.end(), selected) - cards.begin());
        const bool nearSel = sq < (int)cards.size() && (sq == (int)q - 1 || sq == (int)q - cols || sq == (int)q + 1 || sq == (int)q + cols);
        struct CardPixels { std::string key; std::vector<uint32_t> px; };
        static std::vector<CardPixels> cardCache;
        if (cardCache.size() < cards.size()) cardCache.resize(cards.size());
        CardPixels& cc = cardCache[q];
        const std::string ck = it.label + "|" + it.value + "|" + std::to_string((uintptr_t)it.thumb.data()) + "," + std::to_string(it.thumb.size()) + "|" +
            std::to_string(cx) + "," + std::to_string(cy) + "," + std::to_string(cw) + "," + std::to_string(ch) + "|" + std::to_string(c.BackdropSig()) + "|" +
            std::to_string(c.w) + "x" + std::to_string(c.h);
        const int kx0 = std::max(0, cx), ky0 = std::max(0, cy), kx1 = std::min(c.w, cx + cw), ky1 = std::min(c.h, cy + ch);
        if (!sel && !nearSel && cc.key == ck && cc.px.size() == (size_t)std::max(0, kx1 - kx0) * std::max(0, ky1 - ky0) && kx1 > kx0)
        {
            for (int yy = ky0; yy < ky1; yy++) std::copy(&cc.px[(size_t)(yy - ky0) * (kx1 - kx0)], &cc.px[(size_t)(yy - ky0) * (kx1 - kx0)] + (kx1 - kx0), &c.px[(size_t)yy * c.w + kx0]);
            Glass::Mark(c, kx0, ky0, kx1, ky1);
            slotRects.push_back({ cx, cy, cw, ch, i });
            continue;
        }
        if (sel) GlowRound(c, cx, cy, cw, ch, 14 * k, k);
        Glass::Round(c, cx, cy, cw, ch, 14 * k, sel ? RowChosen : empty ? 0x0CFFFFFF : Row, sel ? ChosenEdge : 0, sel ? 2.2f * k : std::max(1.0f, k));
        if (!sel) Glass::Rim(c, cx, cy, cw, ch, 14 * k, std::max(1.0f, k), empty ? 0x30 : 0x50, 0x10);
        // (The picture, inset, its corners rounded by the card's glass round it.)
        const int ix = cx + K(8), iy = cy + K(8), iw = cw - K(16), ih = imgH - K(8);
        if (!it.thumb.empty() && it.thumbW > 0 && it.thumbH > 0)
        {
            // (Filling the box, cut evenly at the sides or top and bottom rather than squeezed.)
            const float sc = std::max((float)iw / it.thumbW, (float)ih / it.thumbH);
            const float ox = (it.thumbW - iw / sc) * 0.5f, oy = (it.thumbH - ih / sc) * 0.5f;
            for (int yy = 0; yy < ih; yy++)
            {
                const int Y = iy + yy;
                if (Y < 0 || Y >= c.h) continue;
                const int sy = std::clamp((int)(oy + yy / sc), 0, it.thumbH - 1);
                for (int xx = 0; xx < iw; xx++)
                {
                    const int X = ix + xx;
                    if (X < 0 || X >= c.w) continue;
                    // (corners: a small radius cut away)
                    const int r = K(8), dx = xx < r ? r - xx : xx >= iw - r ? xx - (iw - r - 1) : 0, dy = yy < r ? r - yy : yy >= ih - r ? yy - (ih - r - 1) : 0;
                    if (dx * dx + dy * dy > r * r) continue;
                    c.px[(size_t)Y * c.w + X] = it.thumb[(size_t)sy * it.thumbW + std::clamp((int)(ox + xx / sc), 0, it.thumbW - 1)] | 0xFF000000u;
                }
            }
            Glass::Mark(c, ix, iy, ix + iw, iy + ih);
        }
        else
        {
            Glass::Round(c, ix, iy, iw, ih, 8 * k, empty ? 0x08FFFFFF : 0x400A111C);
            Glass::DrawIcon(c, empty ? Glass::I_SAVE : Glass::I_PLAY, ix + iw * 0.5f, iy + ih * 0.5f, 13 * k, empty ? GlassHint : GlassText);
        }
        T(c, cx + K(14), cy + imgH + K(26), it.label, K(17), empty && !sel ? GlassHint : GlassText, true);
        T(c, cx + K(14), cy + imgH + K(48), empty ? std::string("Empty") : it.value, K(14), GlassHint);
        slotRects.push_back({ cx, cy, cw, ch, i });
        if (!sel && !nearSel && kx1 > kx0 && ky1 > ky0)
        {
            cc.key = ck;
            cc.px.resize((size_t)(kx1 - kx0) * (ky1 - ky0));
            for (int yy = ky0; yy < ky1; yy++) std::copy(&c.px[(size_t)yy * c.w + kx0], &c.px[(size_t)yy * c.w + kx0] + (kx1 - kx0), &cc.px[(size_t)(yy - ky0) * (kx1 - kx0)]);
        }
    }
}

// The keyboard (Menu::keyGrid): what is typed in a box across the top, the caret after it, then the
// keys in rows. The keys are drawn once and copied after while they stay the same (the menus are
// slow to draw on the Switch); the chosen one, glowing, and those its glow reaches drawn each time.
inline void Menu::DrawKeyGrid(Canvas& c, int x0, int w0, int top, int bottom, float k) const
{
    using namespace SettingsArt;
    auto K = [&](float v) { return (int)std::lround(v * k); };
    keyRects.clear();
    // (What is typed: its end shown when it is longer than the box.)
    const int fh = K(68), fy = top + K(4), tpx = K(26);
    Glass::Round(c, x0, fy, w0, fh, 18 * k, 0x38000000, PanelEdge, std::max(1.0f, k));
    std::string shown = keySecret ? std::string(keyText.size(), '*') : keyText;
    const int room = w0 - K(56);
    while (!shown.empty() && W(shown, tpx, false, false) > room) shown.erase(0, 1);
    const int tx = x0 + K(26), base = fy + fh / 2 + K(9);
    if (shown.empty()) T(c, tx + K(8), base, keyPrompt, K(20), GlassHint);
    else T(c, tx, base, shown, tpx, GlassText, false, false);
    const int caretX = shown.empty() ? tx : tx + W(shown, tpx, false, false) + K(3);
    Glass::Round(c, caretX, fy + K(17), std::max(2, K(3)), fh - K(34), 1.0f, 0xE0FFFFFF);

    // (The keys: ten to a row, as large as fit under the box.)
    int rows = 0;
    for (const MenuItem& it : items) if (it.kind == K_KEY) rows = std::max(rows, it.keyRow + 1);
    if (rows == 0) return;
    const int gy = fy + fh + K(22), gap = K(10);
    const int keyH = std::min(K(72), (bottom - gy - (rows - 1) * gap) / rows);
    const float half = (w0 + gap) / 20.0f;
    auto rectOf = [&](const MenuItem& it, int& x, int& y, int& w, int& h) {
        x = x0 + (int)std::lround(it.keyCol * half);
        w = (int)std::lround(it.keySpan * half) - gap;
        y = gy + it.keyRow * (keyH + gap);
        h = keyH;
    };
    auto drawKey = [&](int i, bool sel) {
        const MenuItem& it = items[i];
        int x, y, w, h;
        rectOf(it, x, y, w, h);
        if (sel) GlowRound(c, x, y, w, h, 14 * k, k);
        Glass::Round(c, x, y, w, h, 14 * k, sel ? RowChosen : Row, sel ? ChosenEdge : 0, sel ? 2.2f * k : std::max(1.0f, k));
        if (!sel) Glass::Rim(c, x, y, w, h, 14 * k, std::max(1.0f, k), 0x50, 0x14);
        const bool word = it.label.size() > 1;
        const int px = word ? K(20) : K(27);
        T(c, x + (w - W(it.label, px, true, false)) / 2, y + h / 2 + px * 7 / 20, it.label, px, GlassText, true, false);
    };
    const int n = (int)items.size();
    const int bx0 = std::max(0, x0 - K(2)), by0 = std::max(0, gy - K(2));
    const int bx1 = std::min(c.w, x0 + w0 + K(2)), by1 = std::min(c.h, gy + rows * (keyH + gap));
    static std::vector<uint32_t> baseKeys;
    static std::string baseKey;
    std::string bk = std::to_string(c.w) + "x" + std::to_string(c.h) + " " + std::to_string(c.BackdropSig()) + " " +
        std::to_string(x0) + "," + std::to_string(w0) + "," + std::to_string(gy) + "," + std::to_string(keyH) + "|";
    for (const MenuItem& it : items) if (it.kind == K_KEY) bk += it.label + "\x1f";
    if (bx1 <= bx0 || by1 <= by0) return;
    const size_t bw = (size_t)(bx1 - bx0);
    if (bk == baseKey && baseKeys.size() == bw * (by1 - by0))
    {
        for (int yy = by0; yy < by1; yy++) std::copy(&baseKeys[(size_t)(yy - by0) * bw], &baseKeys[(size_t)(yy - by0) * bw] + bw, &c.px[(size_t)yy * c.w + bx0]);
        Glass::Mark(c, bx0, by0, bx1, by1);
    }
    else
    {
        for (int i = 0; i < n; i++) if (items[i].kind == K_KEY) drawKey(i, false);
        baseKey = bk;
        baseKeys.resize(bw * (by1 - by0));
        for (int yy = by0; yy < by1; yy++) std::copy(&c.px[(size_t)yy * c.w + bx0], &c.px[(size_t)yy * c.w + bx0] + bw, &baseKeys[(size_t)(yy - by0) * bw]);
    }
    for (int i = 0; i < n; i++)
        if (items[i].kind == K_KEY)
        {
            int x, y, w, h;
            rectOf(items[i], x, y, w, h);
            keyRects.push_back({ x, y, w, h, i });
        }
    if (selected >= 0 && selected < n && items[selected].kind == K_KEY)
    {
        // (The chosen key's glow, then the keys it spreads over again on top of it, then the key.)
        int sx, sy, sw, sh;
        rectOf(items[selected], sx, sy, sw, sh);
        const int reach = K(26);
        GlowRound(c, sx, sy, sw, sh, 14 * k, k);
        for (int i = 0; i < n; i++)
        {
            if (i == selected || items[i].kind != K_KEY) continue;
            int x, y, w, h;
            rectOf(items[i], x, y, w, h);
            if (x < sx + sw + reach && x + w > sx - reach && y < sy + sh + reach && y + h > sy - reach)
            {
                // (Its own place first put back as the copy has it, so its glass is not laid twice.)
                for (int yy = std::max(by0, y - 1); yy < std::min(by1, y + h + 1); yy++)
                {
                    const int a = std::max(bx0, x - 1), b = std::min(bx1, x + w + 1);
                    if (b > a) std::copy(&baseKeys[(size_t)(yy - by0) * bw + (a - bx0)], &baseKeys[(size_t)(yy - by0) * bw + (b - bx0)], &c.px[(size_t)yy * c.w + a]);
                }
            }
        }
        Glass::Round(c, sx, sy, sw, sh, 14 * k, RowChosen, ChosenEdge, 2.2f * k);
        const MenuItem& it = items[selected];
        const int px = it.label.size() > 1 ? K(20) : K(27);
        T(c, sx + (sw - W(it.label, px, true, false)) / 2, sy + sh / 2 + px * 7 / 20, it.label, px, GlassText, true, false);
    }
}

inline void Menu::DrawDialog(Canvas& c, const std::string& title, const std::string& text, const std::string& hint) const
{
    using namespace SettingsArt;
    // (Everything but the two buttons is the same while the dialog is up: drawn once and copied
    // after, as frosting the whole screen each press took the Switch a few hundred ms.)
    static std::vector<uint32_t> base;
    static std::string baseKey;
    static bool baseSolid = false;
    static int basePy = 0, basePh = 0;
    const bool touch = touchRows > 1.0f;
    // (Larger menus: the phone's size, which fills a short screen, and no larger: past it the dialog
    // would not fit.)
    const bool large = !touch && UiLarge();
    const float k = large ? std::max(0.42f, std::min(c.w / 620.0f, c.h / 480.0f) * std::min(1.0f, UiScale() / 1.5f))
                          : std::max(0.42f, std::min(c.w / (touch ? 620.0f : 1400.0f), c.h / (touch ? 480.0f : 788.0f)));
    const std::string key = std::to_string(c.w) + "x" + std::to_string(c.h) + " " + std::to_string(k) + " " + std::to_string(c.BackdropSig()) + " " + std::to_string(dialogIcon) + "|" + std::to_string(touch) + "|" +
        title + "|" + text + "|" + hint + "|" + std::to_string(UiLang::lang);
    auto K = [&](float v) { return (int)std::lround(v * k); };
    const int pw = std::min(c.w - K(32), K(520)); // (the card's width: the same on both paths)
    if (key == baseKey && base.size() == c.px.size())
    {
        std::copy(base.begin(), base.end(), c.px.begin());
        Glass::Mark(c, 0, 0, c.w, c.h);
        cardX = (c.w - pw) / 2; cardY = basePy; cardW = pw; cardH = basePh;
        DrawDialogButtons(c, basePy, basePh, k, baseSolid);
        return;
    }
    const bool solid = Glass::Backdrop(c);
    if (solid) Glass::Frost(c, 0, 0, c.w, c.h, 0);
    else Glass::Round(c, 0, 0, c.w, c.h, 0, GlassShade);
    Glass::Round(c, 0, 0, c.w, c.h, 0, 0x1C000000); // (the game a touch darker behind the card)
    const int innerW = pw - K(64);
    const int bodyFont = K(touch ? 22 : 17), lineHeight = K(touch ? 32 : 25);
    const auto lines = text.empty() ? std::vector<std::string>() : Wrap(text, bodyFont, innerW, 4);
    const int ph = K(touch ? 280 : 250) + (int)lines.size() * lineHeight;
    const int px = (c.w - pw) / 2, py = (c.h - ph) / 2;
    cardX = px; cardY = py; cardW = pw; cardH = ph;
    Glass::Shadow(c, px, py, pw, ph, 26 * k, 30 * k, 0x90);
    if (solid) Glass::Frost(c, px, py, pw, ph, 26 * k);
    Glass::Round(c, px, py, pw, ph, 26 * k, Panel); // (the same glass as the settings pages)
    Glass::Rim(c, px, py, pw, ph, 26 * k, std::max(1.2f, 1.8f * k));
    if (dialogIcon >= 0)
    {
        Glass::Round(c, c.w / 2 - K(30), py + K(26), K(60), K(60), 30 * k, 0x400A111C);
        Glass::Rim(c, c.w / 2 - K(30), py + K(26), K(60), K(60), 30 * k, std::max(1.0f, k), 0x60, 0x14);
        Glass::DrawIcon(c, dialogIcon, c.w * 0.5f, py + K(56), 15 * k, GlassText);
    }
    TCentred(c, c.w / 2, py + K(124), title, K(touch ? 30 : 26), GlassText, true);
    int y = py + K(160);
    for (const std::string& l : lines)
    {
        T(c, c.w / 2 - W(l, bodyFont, false, false) / 2, y, l, bodyFont, Desc, false, false);
        y += lineHeight;
    }
    if (!hint.empty()) TCentred(c, c.w / 2, py + ph + K(34), hint, K(15), GlassHint);
    if (solid) Glass::Opaque(c);
    base = c.px; baseKey = key; baseSolid = solid; basePy = py; basePh = ph;
    DrawDialogButtons(c, py, ph, k, solid);
}

// The dialog's two buttons, the chosen one lit, over the rest of it (DrawDialog).
inline void Menu::DrawDialogButtons(Canvas& c, int py, int ph, float k, bool solid) const
{
    using namespace SettingsArt;
    auto K = [&](float v) { return (int)std::lround(v * k); };
    const int bw = K(190), bh = K(touchRows > 1.0f ? 80 : 48), gap = K(16), bx = c.w / 2 - bw - gap / 2, by = py + ph - bh - K(28);
    const int n = (int)items.size();
    for (int b = 0; b < 2; b++)
    {
        const int item = b == 0 ? n - 1 : 0; // (CANCEL left, the action right)
        if (item < 0 || item >= n) continue;
        const bool sel = item == selected;
        const int x = bx + b * (bw + gap);
        if (sel) Glass::Glow(c, x, by, bw, bh, bh * 0.5f, k);
        Glass::Round(c, x, by, bw, bh, bh * 0.5f, sel ? Pill : PillOff, sel ? 0 : PillOffEdge, std::max(1.0f, k));
        TCentred(c, x + bw / 2, by + bh / 2 + K(7), items[item].label, K(touchRows > 1.0f ? 22 : 18), sel ? PillInk : GlassText, true);
        dialogX[b] = x;
    }
    dialogY = by; dialogW = bw; dialogH = bh;
    // (Solid round the buttons, as Glass::Opaque would make them.)
    if (solid)
        for (int yy = std::max(0, by - bh); yy < std::min(c.h, by + 2 * bh); yy++)
            for (int xx = std::max(0, bx - bh); xx < std::min(c.w, bx + 2 * bw + gap + bh); xx++) c.px[(size_t)yy * c.w + xx] |= 0xFF000000u;
}

// The phone's on-screen controls (x0, y0, x1, y1 each, output pixels; set by main.cpp while they
// show): the live companion keeps clear of them, so nothing it draws sits under a button.
inline std::vector<std::array<int, 4>>& LiveAvoid() { static std::vector<std::array<int, 4>> v; return v; }
// The free band down the screen for something across x0..x1: below the controls in its top half,
// above those in its bottom half (top and bottom as given when none are in the way). gap: the
// space kept from a control.
inline void LiveBand(int x0, int x1, int h, int gap, int& top, int& bottom)
{
    for (const auto& r : LiveAvoid())
    {
        if (r[2] <= x0 || r[0] >= x1) continue;
        if ((r[1] + r[3]) / 2 < h / 2) top = std::max(top, r[3] + gap);
        else bottom = std::min(bottom, r[1] - gap);
    }
}

// The COMPANION WINDOW, live over the game as you play (PokeDaisy's companion on one screen): a
// glass panel down the right of the screen, its tabs as icons along its top, the chosen tab's
// picture under them. Tinted glass with a lit rim, no frost (it is drawn every frame over a moving
// game). tabRects: where each tab was drawn, for taps.
inline void CompanionWindow(Canvas& c, const Menu::CompanionView& cv, int tab, int (*tabRects)[4] = nullptr, int* panelRect = nullptr, bool full = false,
    bool inBattle = false)
{
    using namespace SettingsArt;
    // (A card in the top right corner, about a quarter of the screen across and two thirds down:
    // the game stays the main thing on one screen. full: the whole canvas, a second screen's, over
    // the blurred game already in it.)
    const float margin = std::min(c.w, c.h) * (full ? 0.035f : 0.03f);
    // (Larger menus, on a small screen: taller and wider, its writing larger with it.)
    const bool large = !full && UiLarge();
    int ph = full ? (int)(c.h - 2 * margin) : (int)(c.h * (large ? 0.9f : 0.68f));
    const int pw = full ? (int)(c.w - 2 * margin) : (int)std::min(c.w * (large ? 0.42f : 0.25f), ph * 0.64f);
    const int px = (int)(c.w - pw - margin);
    int py = (int)margin;
    if (!full)
    {
        // (Between the phone's buttons above and below it, when they are in the way: shorter.)
        int top = py, bottom = (int)(c.h - margin);
        LiveBand(px, px + pw, c.h, (int)margin / 2, top, bottom);
        py = top;
        ph = std::max(std::min(ph, bottom - top), (int)(c.h * 0.3f));
    }
    // (Its size as always; in a battle (inBattle) it then ends half way down, above your Pokemon's HP
    // box, which sits from about there in the widened battle.)
    const int sizePh = ph;
    if (!full && inBattle) ph = std::max((int)(c.h * 0.3f), std::min(ph, (int)(c.h * 0.48f) - py));
    const float k = full ? std::max(0.5f, std::min(c.w, c.h) / 560.0f * std::min(UiScale(), 1.3f))
                         : std::max(0.42f, large ? std::min(sizePh / 640.0f, pw / 410.0f) : sizePh / 640.0f);
    auto K = [&](float v) { return (int)std::lround(v * k); };
    if (panelRect) { panelRect[0] = px; panelRect[1] = py; panelRect[2] = pw; panelRect[3] = ph; }
    // (No shadow round it: over the bright game it reads as a dark border.)
    // (Frosted when the game under it has been read: the menus' glass; else dark glass.)
    if (full) { Glass::Frost(c, px, py, pw, ph, 24 * k); Glass::Round(c, px, py, pw, ph, 24 * k, Panel); }
    else if (Glass::FrostLive(c, px, py, pw, ph, 24 * k)) Glass::Round(c, px, py, pw, ph, 24 * k, Panel);
    else Glass::Round(c, px, py, pw, ph, 24 * k, 0xD8101826);
    Glass::Rim(c, px, py, pw, ph, 24 * k, std::max(1.2f, 1.6f * k));
    static const int icons[] = { Glass::I_PEOPLE, Glass::I_LOAD, Glass::I_GLOBE, Glass::I_INFO, Glass::I_SPARKLE, Glass::I_CHEATS };
    static const char* const names[] = { "PARTY", "BAG", "MAP", "POKEDEX", "BATTLE", "GUIDE" };
    const int n = 6, gap = K(6), tw = (pw - K(28) - (n - 1) * gap) / n, th = K(44), ty = py + K(14);
    for (int i = 0; i < n; i++)
    {
        const int tx = px + K(14) + i * (tw + gap);
        const bool on = i == tab;
        Glass::Round(c, tx, ty, tw, th, th * 0.5f, on ? Pill : 0x1AFFFFFF, on ? 0 : 0x26FFFFFF, std::max(1.0f, k));
        Glass::DrawIcon(c, icons[i], tx + tw * 0.5f, ty + th * 0.5f, 10 * k, on ? PillInk : Menu::GlassText);
        if (tabRects) { tabRects[i][0] = tx; tabRects[i][1] = ty; tabRects[i][2] = tw; tabRects[i][3] = th; }
    }
    const int x0 = px + K(20), w0 = pw - K(40);
    int top = ty + th + K(18);
    T(c, x0, top + K(22), names[tab], K(22), Menu::GlassText, true);
    top += K(40);
    const int bottom = py + ph - K(18);
    static const int kinds[] = { Menu::P_CPARTY, Menu::P_CBAG, Menu::P_CPLACE, Menu::P_CDEX };
    if (tab < 4) CompanionPicture(c, cv, kinds[tab], x0, top, w0, tab == 0 ? std::min(bottom, top + 6 * K(68)) : bottom, k); // (the party from the top)
    else if (tab == 4 && !cv.battle.empty()) CompanionPicture(c, cv, Menu::P_CBATTLE, x0, top, w0, bottom, k);
    else
    {
        const char* text = tab == 4 ? (cv.battleMapped ? "Opens by itself when a battle starts: the Pokemon you face, its weaknesses and how your moves do."
                                                       : "Not in this game yet: its battles cannot be read.")
                                    : "Hints for where you are in the game, the answer hidden until you ask.";
        int y = top + K(20);
        for (const std::string& l : Wrap(text, K(16), w0, 6)) { T(c, x0, y, l, K(16), Desc, false, false); y += K(24); }
    }
}

// The PARTY HUD, on the game while you walk: a small glass card for each Pokemon down the left,
// its icon and HP bar (fainted ones dimmed).
// top, scale: where the cards start and their size (main.cpp: below the phone's L button, small
// enough to keep above its D-pad); -1 and 0 for the usual.
inline void PartyHud(Canvas& c, const Menu::CompanionView& cv, int top = -1, float scale = 0)
{
    using namespace SettingsArt;
    const float k = scale > 0 ? scale : std::max(0.42f, std::min(c.w, c.h) / 720.0f * UiScale());
    auto K = [&](float v) { return (int)std::lround(v * k); };
    const int cw = K(132), ch = K(46), gap = K(6), x = K(16);
    int y = top >= 0 ? top : K(16);
    for (const auto& m : cv.party)
    {
        if (Glass::FrostLive(c, x, y, cw, ch, 12 * k)) Glass::Round(c, x, y, cw, ch, 12 * k, Panel);
        else Glass::Round(c, x, y, cw, ch, 12 * k, 0xB0101826);
        Glass::Rim(c, x, y, cw, ch, 12 * k, std::max(1.0f, k), 0x70, 0x18);
        const int is = ch - K(6), ix = x + K(4), iy = y + K(3);
        if (!m.icon.empty() && m.iconW > 0)
        {
            for (int yy = 0; yy < is; yy++)
                for (int xx = 0; xx < is; xx++)
                {
                    const int X = ix + xx, Y = iy + yy;
                    if (X < 0 || Y < 0 || X >= c.w || Y >= c.h) continue;
                    const uint32_t p = m.icon[(size_t)(yy * m.iconH / is) * m.iconW + xx * m.iconW / is];
                    if (p >> 24) c.px[(size_t)Y * c.w + X] = Glass::Over(c.px[(size_t)Y * c.w + X], p | 0xFF000000u, m.hp == 0 && !m.egg ? 0x60 : 0xFF);
                }
            Glass::Mark(c, ix, iy, ix + is, iy + is);
        }
        else Glass::DrawIcon(c, Glass::I_POKEBALL, ix + is * 0.5f, iy + is * 0.5f, is * 0.3f, m.hp == 0 ? Menu::GlassOff : Menu::GlassText);
        const int bx = ix + is + K(6), bw = x + cw - K(10) - bx;
        const std::string lv = m.egg ? UiLang::Tr("Egg") : "Lv " + std::to_string(m.level);
        T(c, bx, y + K(19), lv, K(12), m.hp == 0 && !m.egg ? Menu::GlassOff : Menu::GlassText, true, false);
        if (!m.egg)
        {
            const int bh = std::max(3, K(6)), by = y + K(28);
            const float f = m.maxHp > 0 ? std::clamp((float)m.hp / m.maxHp, 0.0f, 1.0f) : 0.0f;
            const uint32_t col = f > 0.5f ? 0xFF5BD47Au : f > 0.2f ? 0xFFF2C94Cu : 0xFFE5534Bu;
            Glass::Round(c, bx, by, bw, bh, bh * 0.5f, 0x30FFFFFF);
            if (f > 0) Glass::Round(c, bx, by, std::max(bh, (int)(bw * f)), bh, bh * 0.5f, col);
        }
        y += ch + gap;
    }
}

// The STATUS BAR, a slim glass strip at the top of the game while you play (as PokeDaisy's): where
// you are, how many Poke Balls you carry, the time and, on battery, how much is left.
// left: where its left end goes (beside the HUD's cards), 0 to centre it.
// Money as the games show it: "$12,345".
inline std::string MoneyText(long long money)
{
    std::string digits = std::to_string(money < 0 ? 0 : money), out;
    for (size_t i = 0; i < digits.size(); i++)
    {
        if (i && (digits.size() - i) % 3 == 0) out += ',';
        out += digits[i];
    }
    return "$" + out;
}
inline void StatusBar(Canvas& c, const std::string& place, int balls, const std::string& clock, int battery, long long money = -1)
{
    using namespace SettingsArt;
    const float k = std::max(0.42f, std::min(c.w, c.h) / 720.0f * UiScale());
    auto K = [&](float v) { return (int)std::lround(v * k); };
    const int h = K(34), px = K(15);
    // (Each part: an icon and its text; their widths first, to centre the strip.)
    struct Part { int icon; std::string text; };
    std::vector<Part> parts;
    if (!place.empty()) parts.push_back({ Glass::I_GLOBE, place });
    if (balls >= 0) parts.push_back({ Glass::I_POKEBALL, std::to_string(balls) });
    if (money >= 0) parts.push_back({ -3, MoneyText(money) });
    if (!clock.empty()) parts.push_back({ -1, clock });
    if (battery >= 0) parts.push_back({ -2, std::to_string(battery) + "%" });
    if (parts.empty()) return;
    std::vector<int> widths;
    int total = K(16);
    for (const Part& p : parts)
    {
        const int w = (p.icon >= 0 ? K(22) : p.icon == -2 ? K(24) : 0) + W(p.text, px, p.icon == Glass::I_GLOBE, false);
        widths.push_back(w);
        total += w + K(20);
    }
    total -= K(20) - K(16);
    int y0 = K(10);
    {
        int bottom = c.h;
        LiveBand((c.w - total) / 2, (c.w + total) / 2, c.h, K(6), y0, bottom); // (below the phone's buttons over it)
    }
    const int x0 = (c.w - total) / 2;
    if (Glass::FrostLive(c, x0, y0, total, h, h * 0.5f)) Glass::Round(c, x0, y0, total, h, h * 0.5f, Panel);
    else Glass::Round(c, x0, y0, total, h, h * 0.5f, 0xB8101826);
    Glass::Rim(c, x0, y0, total, h, h * 0.5f, std::max(1.0f, k), 0x70, 0x18);
    int x = x0 + K(16);
    const int base = y0 + h / 2 + px * 7 / 20;
    for (size_t i = 0; i < parts.size(); i++)
    {
        const Part& p = parts[i];
        int tx = x;
        if (p.icon >= 0) { Glass::DrawIcon(c, p.icon, x + K(8), y0 + h * 0.5f, 7 * k, Menu::GlassText); tx += K(22); }
        else if (p.icon == -2)
        {
            // (A battery: its outline, filled as far as it is charged.)
            const int bw = K(18), bh = K(10), by = y0 + (h - bh) / 2;
            Glass::Round(c, x, by, bw, bh, 2 * k, 0, 0xC0FFFFFF, std::max(1.0f, 1.2f * k));
            Glass::Round(c, x + bw, by + bh / 4, K(2), bh / 2, 1, 0xC0FFFFFF);
            const int pct = std::clamp(atoi(p.text.c_str()), 0, 100);
            Glass::Round(c, x + K(2), by + K(2), std::max(1, (bw - K(4)) * pct / 100), bh - K(4), 1, pct <= 15 ? 0xFFE5534Bu : 0xFFFFFFFFu);
            tx += K(24);
        }
        T(c, tx, base, p.text, px, i == 0 ? Menu::GlassText : Desc, p.icon == Glass::I_GLOBE, false);
        x += widths[i] + K(20);
        if (i + 1 < parts.size()) Glass::Round(c, x - K(11), y0 + K(10), std::max(1, K(1)), h - K(20), 0, 0x30FFFFFF);
    }
}

// The HUD's minimap, under the party cards: the region map around you, larger, your dot in the
// middle (as the Location preview's, nearer).
inline void MiniMap(Canvas& c, const Menu::CompanionView& cv, int x, int y, int size, float k)
{
    using namespace SettingsArt;
    if (cv.map.empty() || cv.mapW <= 0 || cv.markerX < 0) return;
    Glass::Round(c, x - 3, y - 3, size + 6, size + 6, 14 * k, 0xB8101826);
    // (A third of the map's height across, centred on you, kept inside the map.)
    const int span = std::max(16, cv.mapH / 3);
    int sx = (int)(cv.markerX * cv.mapW) - span / 2, sy = (int)(cv.markerY * cv.mapH) - span / 2;
    sx = std::clamp(sx, 0, std::max(0, cv.mapW - span)); sy = std::clamp(sy, 0, std::max(0, cv.mapH - span));
    const float r = 12 * k;
    for (int yy = 0; yy < size; yy++)
        for (int xx = 0; xx < size; xx++)
        {
            const int X = x + xx, Y = y + yy;
            if (X < 0 || Y < 0 || X >= c.w || Y >= c.h) continue;
            // (Rounded corners.)
            const float qx = std::max(0.0f, std::fabs(xx + 0.5f - size * 0.5f) - (size * 0.5f - r)), qy = std::max(0.0f, std::fabs(yy + 0.5f - size * 0.5f) - (size * 0.5f - r));
            const float cover = qx > 0 && qy > 0 ? std::clamp(r + 0.5f - Glass::Hyp(qx, qy), 0.0f, 1.0f) : 1.0f;
            if (cover <= 0) continue;
            const int mx = std::min(cv.mapW - 1, sx + xx * span / size), my = std::min(cv.mapH - 1, sy + yy * span / size);
            c.px[(size_t)Y * c.w + X] = Glass::Over(c.px[(size_t)Y * c.w + X], cv.map[(size_t)my * cv.mapW + mx] | 0xFF000000u, (int)(cover * 255));
        }
    Glass::Mark(c, x, y, x + size, y + size);
    Glass::Rim(c, x, y, size, size, r, std::max(1.0f, k), 0x90, 0x20);
    const float px = x + (cv.markerX * cv.mapW - sx) * size / span, py = y + (cv.markerY * cv.mapH - sy) * size / span, d = 6 * k;
    Glass::Round(c, (int)(px - d * 2), (int)(py - d * 2), (int)(d * 4), (int)(d * 4), d * 2, 0x40FFFFFF);
    Glass::Round(c, (int)(px - d), (int)(py - d), (int)(d * 2), (int)(d * 2), d, 0xFFE5534Bu, 0xFFFFFFFF, std::max(1.5f, 2 * k));
}

