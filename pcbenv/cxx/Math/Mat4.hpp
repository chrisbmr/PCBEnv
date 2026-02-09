
#ifndef GYM_PCB_MATH_MAT4_H
#define GYM_PCB_MATH_MAT4_H

#include "Geometry.hpp"
#include <cassert>

namespace math
{

class Mat4
{
public:
    Mat4() { }
    Mat4(Real all);
    Mat4(const Real *);

    const Real *data() const { return &m[0]; }

    Real& operator[](uint i) { assert(i < 16); return m[i]; }
    Real operator[](uint i) const { assert(i < 16); return m[i]; }

    Mat4& operator=(const Mat4&);

    Mat4& operator+=(const Mat4&);
    Mat4  operator+(const Mat4&) const;
    Mat4& operator-=(const Mat4&);
    Mat4  operator-(const Mat4&) const;
    Mat4& operator*=(Real);
    Mat4  operator*(Real) const;

    Mat4& operator*=(const Mat4&);
    Mat4  operator*(const Mat4&) const;

    Mat4& setOrtho(const Vector_3 &L_B_N, const Vector_3 &R_T_F);

    std::string str() const;

private:
    Real m[16];

public:
    static const Mat4 ID;
    static const Mat4 ZERO;
};

} // namespace math

#endif // GYM_PCB_MATH_MAT4_H
