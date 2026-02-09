
#ifndef GYM_PCB_MATH_IPOINT3_H
#define GYM_PCB_MATH_IPOINT3_H

#include "Math/IPoint2.hpp"

class IVector_3
{
public:
    int32_t x, y, z;
public:
    IVector_3() { }
    IVector_3(int X, int Y, int Z) : x(X), y(Y), z(Z) { }
    IVector_3(const IVector_2 &v, int Z) : x(v.x), y(v.y), z(Z) { }
    IVector_3(const IVector_3 &v) : x(v.x), y(v.y), z(v.z) { }
    IVector_3& operator=(const IVector_3 &v) { x = v.x; y = v.y; z = v.z; return *this; }
    bool operator==(const IVector_3 &v) const { return x == v.x && y == v.y && z == v.z; }
    bool operator!=(const IVector_3 &v) const { return x != v.x || y != v.y || z != v.z; }
    IVector_3 operator+(const IVector_3 &v) const { return IVector_3(x + v.x, y + v.y, z + v.z); }
    IVector_3 operator-(const IVector_3 &v) const { return IVector_3(x - v.x, y - v.y, z - v.z); }
    IVector_3 operator*(int s) const { return IVector_3(x * s, y * s, z * s); }
    IVector_3 operator/(int s) const { return IVector_3((x + s - 1) / s, (y + s - 1) / s, (z + s - 1) / s); }
    IVector_3& operator+=(const IVector_3 &v) { x += v.x; y += v.y; z += v.z; return *this; }
    IVector_3& operator-=(const IVector_3 &v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    IVector_3& operator*=(int s) { *this = *this * s; return *this; }
    IVector_3& operator/=(int s) { *this = *this / s; return *this; }
    IVector_3 abs() const { return IVector_3(std::abs(x), std::abs(y), std::abs(z)); }
    int32_t dot(const IVector_3 &v) const { return x * v.x + y * v.y + z * v.z; }
    int32_t norm1() const { return std::abs(x) + std::abs(y) + std::abs(z); }
    int32_t hprod() const { return x * y * z; }
    IVector_3 max_cw(const IVector_3 &v) const { return IVector_3(std::max(x, v.x), std::max(y, v.y), std::max(z, v.z)); }
    IVector_3 min_cw(const IVector_3 &v) const { return IVector_3(std::min(x, v.x), std::min(y, v.y), std::min(z, v.z)); }
};

class IPoint_3
{
public:
    int32_t x, y, z;
public:
    IPoint_3() { }
    constexpr IPoint_3(int X, int Y, int Z) : x(X), y(Y), z(Z) { }
    IPoint_3(const IVector_3 &v) : x(v.x), y(v.y), z(v.z) { }
    IPoint_3(const int *v) : x(v[0]), y(v[1]), z(v[2]) { }
    IPoint_3(const uint *v) : x(v[0]), y(v[1]), z(v[2]) { }
    IPoint_3& operator=(const IPoint_3 &P) { x = P.x; y = P.y; z = P.z; return *this; }
    bool operator==(const IPoint_3 &P) const { return x == P.x && y == P.y && z == P.z; }
    bool operator!=(const IPoint_3 &P) const { return x != P.x || y != P.y || z != P.z; }
    bool isPositive() const { return x >= 0 && y >= 0 && z >= 0; }
    uint index(const IVector_3 &size) const { assert(isPositive()); return z * size.z + y * size.y + x * size.x; }
    IPoint_2 xy() const { return IPoint_2(x, y); }
    IPoint_3 operator+(const IVector_3 &v) const { return IPoint_3(x + v.x, y + v.y, z + v.z); }
    IPoint_3 operator-(const IVector_3 &v) const { return IPoint_3(x - v.x, y - v.y, z - v.z); }
    IVector_3 operator-(const IPoint_3 &P) const { return IVector_3(x - P.x, y - P.y, z - P.z); }
    IPoint_3& operator+=(const IVector_3 &v) { x += v.x; y += v.y; z += v.z; return *this; }
    IPoint_3& operator-=(const IVector_3 &v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    IVector_3 vector() const { return IVector_3(x, y, z); }
    constexpr IPoint_3 min(const IPoint_3 &P) const { return IPoint_3(std::min(x,P.x), std::min(y,P.y), std::min(z,P.z)); }
    constexpr IPoint_3 max(const IPoint_3 &P) const { return IPoint_3(std::max(x,P.x), std::max(y,P.y), std::max(z,P.z)); }
};

/**
 * 3D integer box.
 *
 * Ensure that min <= max cwise.
 */
struct IBox_3
{
    IPoint_3 min;
    IPoint_3 max;
    IBox_3() { }
    constexpr IBox_3(const IPoint_3 &A, const IPoint_3 &B) : min(A.min(B)), max(A.max(B)) { }
    bool valid() const { return min.x <= max.x && min.y <= max.y && min.z <= max.z; }
    uint w() const { return max.x - min.x + 1; }
    uint h() const { return max.y - min.y + 1; }
    uint d() const { return max.z - min.z + 1; }
    int64_t volume() const { return w() * h() * d(); }
    constexpr static const IBox_3 EMPTY() { return IBox_3(IPoint_3(1,1,1), IPoint_3(0,0,0)); }
};

inline std::ostream& operator<<(std::ostream &os, const IVector_3 &V)
{
    return os << '(' << V.x << ',' << V.y << ',' << V.z << ')';
}
inline std::ostream& operator<<(std::ostream &os, const IPoint_3 &P)
{
    return os << '(' << P.x << ',' << P.y << ',' << P.z << ')';
}
inline std::ostream& operator<<(std::ostream &os, const IBox_3 &B)
{
    return os << '[' << B.min.x << ',' << B.min.y << ',' << B.min.z << "][" << B.max.x << ',' << B.max.y << ',' << B.max.z << ']';
}

#endif // GYM_PCB_MATH_IPOINT3_H
