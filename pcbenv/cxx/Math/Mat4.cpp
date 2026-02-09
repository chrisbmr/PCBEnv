
#include "Math/Mat4.hpp"
#include <cstring>
#include <sstream>

namespace
{
const Real ID4x4[16] = { 1, 0, 0, 0,
                         0, 1, 0, 0,
                         0, 0, 1, 0,
                         0, 0, 0, 1 };
}

namespace math
{

const Mat4 Mat4::ID(ID4x4);
const Mat4 Mat4::ZERO(0.0);

Mat4::Mat4(const Real *data)
{
    std::memcpy(&m[0], data, sizeof(m));
}

Mat4::Mat4(Real all)
{
    for (uint i = 0; i < 16; ++i)
        m[i] = all;
}

Mat4& Mat4::operator=(const Mat4 &A)
{
    std::memcpy(&m[0], &A.m[0], sizeof(m));
    return *this;
}

Mat4& Mat4::operator+=(const Mat4 &A)
{
    for (uint i = 0; i < 16; ++i)
        m[i] += A.m[i];
    return *this;
}
Mat4 Mat4::operator+(const Mat4 &A) const
{
    Mat4 B;
    for (uint i = 0; i < 16; ++i)
        B.m[i] = m[i] + A.m[i];
    return B;
}
Mat4& Mat4::operator-=(const Mat4 &A)
{
    for (uint i = 0; i < 16; ++i)
        m[i] -= A.m[i];
    return *this;
}
Mat4 Mat4::operator-(const Mat4 &A) const
{
    Mat4 B;
    for (uint i = 0; i < 16; ++i)
        B.m[i] = m[i] - A.m[i];
    return B;
}
Mat4& Mat4::operator*=(Real s)
{
    for (uint i = 0; i < 16; ++i)
        m[i] *= s;
    return *this;
}
Mat4 Mat4::operator*(Real s) const
{
    Mat4 B;
    for (uint i = 0; i < 16; ++i)
        B.m[i] = m[i] * s;
    return B;
}

Mat4& Mat4::operator*=(const Mat4 &A)
{
    Mat4 B = *this * A;
    *this = B;
    return *this;
}
Mat4 Mat4::operator*(const Mat4 &A) const
{
    Mat4 B;
    for (uint c = 0; c < 4; ++c) {
    for (uint r = 0; r < 4; ++r) {
        B[c*4+r] = m[r] * A.m[c*4] + m[4+r] * A.m[c*4+1] + m[8+r] * A.m[c*4+2] + m[12+r] * A.m[c*4+3];
    }}
    return B;
}

Mat4& Mat4::setOrtho(const Vector_3 &v0, const Vector_3 &v1)
{
    m[0] = 2.0 / (v1.x() - v0.x());
    m[1] = 0;
    m[2] = 0;
    m[3] = 0;

    m[4] = 0;
    m[5] = 2.0 / (v1.y() - v0.y());
    m[6] = 0;
    m[7] = 0;

    m[8] = 0;
    m[9] = 0;
    m[10] = 2.0 / (v1.z() - v0.z());
    m[11] = 0;

    m[12] = -(v1.x() + v0.x()) / (v1.x() - v0.x());
    m[13] = -(v1.y() + v0.y()) / (v1.y() - v0.y());
    m[14] = -(v1.z() + v0.z()) / (v1.z() - v0.z());
    m[15] = 1;

    // EXAMPLE:
    // x0 = 10
    // x1 = 20
    // sx = 2 / (20 - 10) = 0.2
    // dx = -30 / 10 = -3
    // (sx dx) * (15) = (15 * 0.2 - 3) = (0)
    // ( 0  1)   ( 1)              (1)   (1)

    return *this;
}

std::string Mat4::str() const
{
    std::stringstream ss;
    for (uint i = 0; i < 4; ++i)
        ss << '(' << m[i] << ' ' << m[4+i] << ' ' << m[8+i] << ' ' << m[12+i] << (i < 3 ? ")\n" : ")");
    return ss.str();
}

} // namespace math
