
#ifndef GYM_PCB_COLOR_H
#define GYM_PCB_COLOR_H

#include "Defs.hpp"
#include <string_view>

class Color
{
public:
    Color() { }
    Color(std::string_view);
    constexpr Color(uint32_t abgr) : mABGR(abgr) { }
    constexpr Color(uint8_t R, uint8_t G, uint8_t B, uint8_t A = 255) : mABGR((A << 24) | (B << 16) | (G << 8) | R) { }
    uint32_t ABGR8() const { return mABGR; }
    Color& operator=(const Color &c) { mABGR = c.mABGR; return *this; }
    bool operator==(const Color &c) const { return mABGR == c.mABGR; }
    bool isVisible() const { return mABGR & 0xff000000; }
    void getFloat4(float *) const;
    Color withAlpha(float a) const { return Color((mABGR & 0xffffff) | (uint(a * 255.0f) << 24)); }
    std::string str() const;
    static Color HSV(int h, float s, float v, float a = 1.0f);
    static Color RGBA(float r, float g, float b, float a);
    static const Color TRANSPARENT;
    static const Color BLACK;
    static const Color WHITE;
    static const Color RED;
    static const Color GREEN;
    static const Color BLUE;
    static const Color CYAN;
    static const Color MAGENTA;
    static const Color YELLOW;
    static const Color ORANGE;
    static const Color PINK;
    static const Color POISON;
    static const Color MINT;
    static const Color SKY;
private:
    uint32_t mABGR{0};
};

inline constexpr const Color Color::TRANSPARENT(0x00000000);
inline constexpr const Color Color::BLACK(0xff000000);
inline constexpr const Color Color::WHITE(0xffffffff);
inline constexpr const Color Color::RED(0xff0000ff);
inline constexpr const Color Color::GREEN(0xff00ff00);
inline constexpr const Color Color::BLUE(0xffff0000);
inline constexpr const Color Color::CYAN(0xffffff00);
inline constexpr const Color Color::MAGENTA(0xffff00ff);
inline constexpr const Color Color::YELLOW(0xff00ffff);
inline constexpr const Color Color::ORANGE(0xff0080ff);
inline constexpr const Color Color::PINK(0xff8000ff);
inline constexpr const Color Color::POISON(0xff00ff80);
inline constexpr const Color Color::MINT(0xff80ff00);
inline constexpr const Color Color::SKY(0xffff8000);

class Palette
{
    constexpr static const uint MaxColors = 1024;
public:
    Palette() { }
    Palette(const Color *, uint);

    void setColor(uint, Color);

    uint size() const { return mColors.size(); }

    Color operator[](uint i) const { return mColors[i % static_cast<uint>(mColors.size())]; }
public:
    static Color FootprintLine[2];
    static Color FootprintFill[2];
    static Color PinLine[2][2]; // [-+net][z]
    static Color PinFill[2][2]; // [-+net][z]
    static Color PowerNet;
    static Color GroundNet;
    static Palette Nets;
    static Palette Tracks;
    static const uint Hues[16];
private:
    std::vector<Color> mColors;
};

inline Color Color::RGBA(float r, float g, float b, float a)
{
    int32_t R = r * 255.0f;
    int32_t G = g * 255.0f;
    int32_t B = b * 255.0f;
    int32_t A = a * 255.0f;
    return Color((A << 24) | (B << 16) | (G << 8) | R);
};

#endif // GYM_PCB_COLOR_H
