#pragma once

#include "../Precompiled.h"

class Color {
public:
    Color() : r(255), g(255), b(255), a(255) {}
    Color(uint8 r_, uint8 g_, uint8 b_) : r(r_), g(g_), b(b_), a(255) {}
    Color(uint8 r_, uint8 g_, uint8 b_, uint8 a_) : r(r_), g(g_), b(b_), a(a_) {}
    Color(uint32 color) : r(color >> 24), g(color >> 16), b(color >> 8), a(color) {}

public:
    uint32 GetAsUINTSwap()
    {
        return (b << 24) | (g << 16) | (r << 8) | a;
    }

    uint32 GetAsUINT()
    {
        return (r << 24) | (g << 16) | (b << 8) | a;
    }

public:
    uint8 r, g, b, a;

public:
    static const Color New;
};