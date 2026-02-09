
#include "Log.hpp"
#include "Color.hpp"
#include <cmath>
#include <cassert>

Color::Color(std::string_view sv)
{
    if (sv.starts_with("0x"))
        sv.remove_prefix(2);
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), mABGR, 16);
    if (ec != std::errc())
        ERROR("could not parse color value " << sv);
    if (sv.size() <= 4) {
        uint32_t a = ((mABGR >> 12) & 0xf) * 0x11;
        uint32_t b = ((mABGR >>  8) & 0xf) * 0x11;
        uint32_t g = ((mABGR >>  4) & 0xf) * 0x11;
        uint32_t r = ((mABGR >>  0) & 0xf) * 0x11;
        mABGR = (a << 24) | (b << 16) | (g << 8) | r;
    }
}

Palette Palette::Nets(0, 1);
Palette Palette::Tracks(0, 1);

Color Palette::PowerNet;
Color Palette::GroundNet;
Color Palette::FootprintLine[2];
Color Palette::FootprintFill[2];
Color Palette::PinLine[2][2];
Color Palette::PinFill[2][2];

Palette::Palette(const Color *source, uint size)
{
    mColors.resize(size);
    if (source) {
        mColors.assign(source, source + size);
    } else {
        for (uint n = 0, s = 4; s > 0; --s)
            for (uint i = 0; i < 16 && n < size; ++i)
                mColors[n++] = Color::HSV(Hues[i], float(s) / 4.0f, 1.0f);
    }
}

void Palette::setColor(uint i, Color c)
{
    if (i >= MaxColors)
        throw std::invalid_argument(fmt::format("color index {} must be < {}", i, MaxColors));
    if (mColors.size() <= i)
        mColors.resize(i + 1, Color::WHITE);
    if (i < mColors.size())
        mColors[i] = c;
}

const uint Palette::Hues[16] =
{
      0,
     90,
    180,
    270,
     30,
    120,
    210,
    300,
     60,
    150,
    240,
    330
};

Color Color::HSV(int H, float s, float v, float a)
{
    assert(H >= 0 && H <= 360);
    assert(s >= 0.0f && s <= 1.0f);
    assert(v >= 0.0f && v <= 1.0f);
    float f = H / 60.0f;
    int h = (int)f;
    f -= (float)h;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));
    float r, g, b;
    switch (h) {
    default:
        assert(0);
        return Color::WHITE;
    case 0:
    case 6: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
    }
    return Color::RGBA(r, g, b, a);
}

void Color::getFloat4(float *rgba) const
{
    rgba[0] = float((mABGR >>  0) & 0xff) * (1.0f / 255.0f);
    rgba[1] = float((mABGR >>  8) & 0xff) * (1.0f / 255.0f);
    rgba[2] = float((mABGR >> 16) & 0xff) * (1.0f / 255.0f);
    rgba[3] = float((mABGR >> 24) & 0xff) * (1.0f / 255.0f);
}

std::string Color::str() const
{
    float f[4];
    getFloat4(f);
    return fmt::format("RGBA({:.2f},{:.2f},{:.2f},{:.2f})", f[0], f[1], f[2], f[3]);
}
