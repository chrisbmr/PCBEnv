
#include "GridDirection.hpp"

const uint8_t GridDirection::sOpposites[11] =
{
    4, 5, 6, 7,
    0, 1, 2, 3,
    9, 8,
    10
};

const int GridDirection::dXYZ[11][3] =
{
    {  0,  1,  0 },
    {  1,  1,  0 },
    {  1,  0,  0 },
    {  1, -1,  0 },
    {  0, -1,  0 },
    { -1, -1,  0 },
    { -1,  0,  0 },
    { -1,  1,  0 },
    {  0,  0,  1 },
    {  0,  0, -1 },
    {  0,  0,  0 }
};

const Vector_2 GridDirection::sVec[11] =
{
    Vector_2(0.0, 1.0),
    Vector_2(std::sqrt(2.0), std::sqrt(2.0)),
    Vector_2(1.0, 0.0),
    Vector_2(std::sqrt(2.0), -std::sqrt(2.0)),
    Vector_2(0.0, -1.0),
    Vector_2(-std::sqrt(2.0), -std::sqrt(2.0)),
    Vector_2(-1.0, 0.0),
    Vector_2(-std::sqrt(2.0), std::sqrt(2.0)),
    Vector_2(0.0, 0.0),
    Vector_2(0.0, 0.0),
    Vector_2(0.0, 0.0)
};

const char *GridDirection::sStrings[11] =
{
    "U", "UR", "R", "DR", "D", "DL", "L", "UL", "A", "V", "Z"
};

GridDirection GridDirection::fromSigns(const Vector_2 &v, Real zeroThreshold)
{
    const auto x = v.x();
    const auto y = v.y();
    if (std::abs(x) < zeroThreshold) {
        if (std::abs(y) < zeroThreshold)
            return GridDirection::Z();
        return (y < 0.0) ? GridDirection::D() : GridDirection::U();
    } else if (std::abs(y) < zeroThreshold) {
        return (x < 0.0) ? GridDirection::L() : GridDirection::R();
    } else if (x < 0.0) {
        return (y < 0.0) ? GridDirection::DL() : GridDirection::UL();
    } else {
        return (y < 0.0) ? GridDirection::DR() : GridDirection::UR();
    }
}

GridDirection GridDirection::Degrees(int deg)
{
    if (deg < 0)
        return (2 - ((deg * 2 - 44) / 90)) & 7;
    return (2 - ((deg * 2 + 44) / 90)) & 7;
}
GridDirection GridDirection::rotatedCcw(int ddeg) const
{
    return Degrees(deg() + ddeg);
}

GridDirection::GridDirection(const IVector_3 &dv)
{
    const int dx = dv.x;
    const int dy = dv.y;
    const int dz = dv.z;
    if (dz == 0) {
        switch (dx) {
        case -1:
            switch (dy) {
            case -1: id = GridDirection::DL().n(); return;
            case  0: id = GridDirection::L().n(); return;
            case +1: id = GridDirection::UL().n(); return;
            default: id = GridDirection::Z().n(); return;
            }
        case 0:
            switch (dy) {
            case -1: id = GridDirection::D().n(); return;
            case +1: id = GridDirection::U().n(); return;
            default: id = GridDirection::Z().n(); return;
            }
        case +1:
            switch (dy) {
            case -1: id = GridDirection::DR().n(); return;
            case  0: id = GridDirection::R().n(); return;
            case +1: id = GridDirection::UR().n(); return;
            default: id = GridDirection::Z().n(); return;
            }
        default: id = GridDirection::Z().n(); return;
        }
    } else {
        if (dx != 0 || dy != 0)
            id = GridDirection::Z().n();
        else if (dz == 1)
            id = GridDirection::A().n();
        else if (dz == -1)
            id = GridDirection::V().n();
        else
            id = GridDirection::Z().n();
    }
}
