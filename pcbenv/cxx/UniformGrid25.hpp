#ifndef GYM_PCB_UNIFORMGRID25_H
#define GYM_PCB_UNIFORMGRID25_H

#include "Geometry.hpp"
#include "Math/IPoint3.hpp"
#include "UserSettings.hpp"

class UniformGrid25
{
public:
    const Real EdgeLen, EdgeLen05, EdgeLenRcp; // Compiler should realize that these are 1/0.5 in NavGrid!
public:
    UniformGrid25(Real edgeLen) : EdgeLen(edgeLen), EdgeLen05(edgeLen * 0.5), EdgeLenRcp(1.0 / EdgeLen) { }

    IVector_3 getSize() const { return IVector_3(mSize[0], mSize[1], mSize[2]); }
    uint getSize(uint d) const { assert(d < 3); return mSize[d]; }
    const Bbox_2& getBbox() const { return mBbox; }

    Real XCoord(int x) const { return x * EdgeLen + mBbox.xmin(); }
    Real YCoord(int y) const { return y * EdgeLen + mBbox.ymin(); }
    Point_2 Coords(const IPoint_2 &v) const { return Coords(v.x, v.y); }
    Point_2 Coords(int x, int y) const { return Point_2(XCoord(x), YCoord(y)); }
    Point_25 Coords(const IPoint_3 &v) const { return Coords(v.x, v.y, v.z); }
    Point_25 Coords(int x, int y, uint z) const { assert(z < mSize[2]); return Point_25(XCoord(x), YCoord(y), z); }

    Vector_2 MidPointOffset() const { return Vector_2(EdgeLen05, EdgeLen05); }
    Real MidPointX(int x) const { return XCoord(x) + MidPointOffset().x(); }
    Real MidPointY(int y) const { return YCoord(y) + MidPointOffset().y(); }
    Point_2 MidPoint(const IPoint_2 &v) const { return MidPoint(v.x, v.y); }
    Point_2 MidPoint(int x, int y) const { return Coords(x,y) + MidPointOffset(); }
    Point_25 MidPoint(const IPoint_3 &v) const { return Coords(v) + MidPointOffset(); }

    IVector_2 HIndices(const Point_2 &v) const { return IVector_2(XIndex(v.x()), YIndex(v.y())); }
    int  XIndex(Real x, Real ex = 0.0) const;
    int  YIndex(Real y, Real ey = 0.0) const;
    uint XIndexBounded(Real x, Real ex = 0.0) const;
    uint YIndexBounded(Real y, Real ey = 0.0) const;
    uint LinearIndex(uint z, uint y, uint x) const { return z * mStrideZ + y * mStrideY + x; }
    uint LinearIndex(const Point_25 &v) const { return LinearIndex(XIndexBounded(v.x()), YIndexBounded(v.y()), 0); }
    IBox_3 getBox(const Bbox_2&, uint z0 = 0, uint z1 = std::numeric_limits<uint>::max()) const;

    Real XfIndex(Real x, Real ex = 0.0) const;
    Real YfIndex(Real y, Real ey = 0.0) const;

    uint getNumPoints2D() const { return mSize[0] * mSize[1]; }
    uint getNumPoints3D() const { return mNumPoints3D; }

    bool inside(const IPoint_3 &v) const { return inside(v.x, v.y, v.z); }
    bool inside(uint x, uint y, uint z) const;

    Point_25 snapToMidPoint(const Point_25&) const;

protected:
    Bbox_2 mBbox;
    uint mSize[3];
    uint mStrideZ;
    uint mStrideY;
    uint mNumPoints3D;

    void calcNumPoints3D();
};

inline Real UniformGrid25::XfIndex(Real x, Real ex) const
{
    return (x + ex - mBbox.xmin()) * EdgeLenRcp;
}
inline Real UniformGrid25::YfIndex(Real y, Real ey) const
{
    return (y + ey - mBbox.ymin()) * EdgeLenRcp;
}

inline int UniformGrid25::XIndex(Real x, Real ex) const
{
    return int(XfIndex(x, ex));
}
inline int UniformGrid25::YIndex(Real y, Real ey) const
{
    return int(YfIndex(y, ey));
}
inline uint UniformGrid25::XIndexBounded(Real x, Real ex) const
{
    return std::max(0, std::min(XIndex(x, ex), int(mSize[0] - 1)));
}
inline uint UniformGrid25::YIndexBounded(Real y, Real ey) const
{
    return std::max(0, std::min(YIndex(y, ey), int(mSize[1] - 1)));
}
inline bool UniformGrid25::inside(uint x, uint y, uint z) const
{
    return (x < mSize[0]) && (y < mSize[1]) && (z < mSize[2]);
}

inline Point_25 UniformGrid25::snapToMidPoint(const Point_25 &v) const
{
    return Point_25(std::floor(v.x() - mBbox.xmin()) + EdgeLen05 + mBbox.xmin(),
                    std::floor(v.y() - mBbox.ymin()) + EdgeLen05 + mBbox.ymin(),
                    v.z());
}

inline void UniformGrid25::calcNumPoints3D()
{
    uint64_t N = (uint64_t(mSize[0]) * mSize[1]) * mSize[2];
    if (N > UserSettings::get().MaxGridCells || N > std::numeric_limits<uint>::max())
        throw std::runtime_error("grid is too large, reduce the resolution");
    mNumPoints3D = N;
}

inline IBox_3 UniformGrid25::getBox(const Bbox_2 &bbox, uint z0, uint z1) const
{
    return IBox_3(IPoint_3(XIndexBounded(bbox.xmin()), YIndexBounded(bbox.ymin()), std::min(z0, mSize[2] - 1)),
                  IPoint_3(XIndexBounded(bbox.xmax()), YIndexBounded(bbox.ymax()), std::min(z1, mSize[2] - 1)));
}

#endif // GYM_PCB_UNIFORMGRID25_H
