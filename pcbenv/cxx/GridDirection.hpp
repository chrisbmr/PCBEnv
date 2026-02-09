
#ifndef GYM_PCB_GRIDDIRECTION_H
#define GYM_PCB_GRIDDIRECTION_H

class GridDirection
{
public:
    constexpr static const uint FirstVertical = 8;
    constexpr static const uint Count = 10;
    constexpr GridDirection(uint8_t i) : id(i) { assert(id <= 0xa); }
    constexpr GridDirection() { }
    GridDirection(const IVector_3&);
    uint n() const { return id; }
    constexpr uint32_t mask() const { return (1 << id); }
    bool _isDiagonal() const { assert(!isVertical()); return id & 0x1; }
    bool isDiagonal() const { return (id & 0x1) && (id < 8); }
    bool isZero() const { return id == 0xa; }
    bool isX() const { return 0x44 & (1 << id); }
    bool isY() const { return 0x11 & (1 << id); }
    bool isVertical() const { return (id == 8) || (id == 9); }
    bool is2D() const { return id < 8; }
    bool operator==(const GridDirection &d) const { return id == d.id; }
    GridDirection& operator++() { id += 1; return *this; }
    GridDirection& operator=(const GridDirection &d) { id = d.id; return *this; }
    int dx() const { return dXYZ[id][0]; }
    int dy() const { return dXYZ[id][1]; }
    int dz() const { return dXYZ[id][2]; }
    IVector_3 ivec3() const { return IVector_3(dXYZ[id][0], dXYZ[id][1], dXYZ[id][2]); }
    Vector_2 vec2() const { assert(id <= 7); return sVec[id]; }
    int deg() const { assert(id <= 7); return 90 - id * 45 + (id >= 3 ? 0 : 360); }
    GridDirection opposite() const { return GridDirection(sOpposites[id]); }
    GridDirection rotatedCcw45(uint n = 1) const { assert(id < 8); return GridDirection((id - n) & 0x7); }
    GridDirection rotatedCw45(uint n = 1) const { assert(id < 8); return GridDirection((id + n) & 0x7); }
    GridDirection rotatedCcw(int degrees) const;
    uint8_t get45DegreeStepsBetween(const GridDirection &d) const;
    const char *str() const { return sStrings[std::min(id, uint8_t(10))]; }
    static GridDirection Degrees(int); // 0° == R, ccw
    static GridDirection fromSigns(const Vector_2&, Real zeroThreshold = 0x1.0p-10);
    constexpr static GridDirection begin() { return GridDirection(0); }
    constexpr static GridDirection hend() { return GridDirection(8); }
    constexpr static GridDirection vend() { return GridDirection(10); }
    constexpr static GridDirection U() { return GridDirection(0); }  // 0000 Y
    constexpr static GridDirection UR() { return GridDirection(1); } // 0001 D
    constexpr static GridDirection R() { return GridDirection(2); }  // 0010 X
    constexpr static GridDirection DR() { return GridDirection(3); } // 0011 D
    constexpr static GridDirection D() { return GridDirection(4); }  // 0100 Y
    constexpr static GridDirection DL() { return GridDirection(5); } // 0101 D
    constexpr static GridDirection L() { return GridDirection(6); }  // 0110 X
    constexpr static GridDirection UL() { return GridDirection(7); } // 0111 D
    constexpr static GridDirection A() { return GridDirection(8); }  // 1000
    constexpr static GridDirection V() { return GridDirection(9); }  // 1001
    constexpr static GridDirection Z() { return GridDirection(10); } // 1010
    static const int dXYZ[11][3];
    static const Vector_2 sVec[11];
    static const uint8_t sOpposites[11];
    static const char *sStrings[11];
private:
    uint8_t id;
};
inline uint8_t GridDirection::get45DegreeStepsBetween(const GridDirection &d) const
{
    if ((id | d.id) & 0x8)
        return 0;
    auto n = std::abs(id - d.id);
    return (n > 4) ? (uint8_t(8) - n) : n;
}

#endif // GYM_PCB_GRIDDIRECTION_H

