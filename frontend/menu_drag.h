#pragma once
#include <cstdlib>

// A settings row is tapped only on release, after a swipe has been ruled out.
struct MenuDrag
{
    bool active = false, dragging = false;
    int startY = 0, startScroll = 0;

    void Begin(int y, int scroll)
    {
        active = true;
        dragging = false;
        startY = y;
        startScroll = scroll;
    }

    bool Move(int y, int threshold, int& scroll)
    {
        if (!active) return false;
        if (std::abs(y - startY) > threshold) dragging = true;
        if (dragging) scroll = startScroll - (y - startY);
        return dragging;
    }

    bool End()
    {
        const bool tap = active && !dragging;
        active = false;
        return tap;
    }
};
