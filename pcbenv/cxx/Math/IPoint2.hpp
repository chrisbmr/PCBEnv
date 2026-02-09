
#ifndef GYM_PCB_MATH_IPOINT2
#define GYM_PCB_MATH_IPOINT2

#include "Defs.hpp"
#include <cmath>
#include <iostream>

class IVector_2
{
public:
    int32_t x, y;
public:
    IVector_2() { }
    IVector_2(int X, int Y) : x(X), y(Y) { }
    IVector_2(const IVector_2 &v) : x(v.x), y(v.y) { }
    IVector_2& operator=(const IVector_2 &v) { x = v.x; y = v.y; return *this; }
    bool operator==(const IVector_2 &v) const { return x == v.x && y == v.y; }
    bool operator!=(const IVector_2 &v) const { return x != v.x || y != v.y; }
    IVector_2 operator+(const IVector_2 &v) const { return IVector_2(x + v.x, y + v.y); }
    IVector_2 operator-(const IVector_2 &v) const { return IVector_2(x - v.x, y - v.y); }
    IVector_2 operator*(int s) const { return IVector_2(x * s, y * s); }
    IVector_2 operator/(int s) const { return IVector_2((x + s - 1) / s, (y + s - 1) / s); }
    IVector_2& operator+=(const IVector_2 &v) { x += v.x; y += v.y; return *this; }
    IVector_2& operator-=(const IVector_2 &v) { x -= v.x; y -= v.y; return *this; }
    IVector_2& operator*=(int s) { *this = *this * s; return *this; }
    IVector_2& operator/=(int s) { *this = *this / s; return *this; }
    IVector_2 abs() const { return IVector_2(std::abs(x), std::abs(y)); }
    int32_t dot(const IVector_2 &v) const { return x * v.x + y * v.y; }
    int32_t norm1() const { return std::abs(x) + std::abs(y); }
    int32_t hprod() const { return x * y; }
    IVector_2 max_cw(const IVector_2 &v) const { return IVector_2(std::max(x, v.x), std::max(y, v.y)); }
    IVector_2 min_cw(const IVector_2 &v) const { return IVector_2(std::min(x, v.x), std::min(y, v.y)); }
};

class IPoint_2
{
public:
    int32_t x, y;
public:
    IPoint_2() { }
    IPoint_2(int X, int Y) : x(X), y(Y) { }
    IPoint_2(const IVector_2 &v) : x(v.x), y(v.y) { }
    IPoint_2(const int *v) : x(v[0]), y(v[1]) { }
    IPoint_2(const uint *v) : x(v[0]), y(v[1]) { }
    IPoint_2& operator=(const IPoint_2 &P) { x = P.x; y = P.y; return *this; }
    bool operator==(const IPoint_2 &P) const { return x == P.x && y == P.y; }
    bool operator!=(const IPoint_2 &P) const { return x != P.x || y != P.y; }
    bool isPositive() const { return x >= 0 && y >= 0; }
    IPoint_2 operator+(const IVector_2 &v) const { return IPoint_2(x + v.x, y + v.y); }
    IPoint_2 operator-(const IVector_2 &v) const { return IPoint_2(x - v.x, y - v.y); }
    IPoint_2& operator+=(const IVector_2 &v) { x += v.x; y += v.y; return *this; }
    IPoint_2& operator-=(const IVector_2 &v) { x -= v.x; y -= v.y; return *this; }
    IVector_2 vector() const { return IVector_2(x, y); }
};

inline std::ostream& operator<<(std::ostream &os, const IVector_2 &v)
{
    return os << '(' << v.x << ',' << v.y << ')';
}
inline std::ostream& operator<<(std::ostream &os, const IPoint_2 &P)
{
    return os << '(' << P.x << ',' << P.y << ')';
}

#endif // GYM_PCB_MATH_IPOINT2
