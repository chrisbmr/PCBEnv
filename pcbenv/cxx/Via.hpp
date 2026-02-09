#ifndef GYM_PCB_VIA_H
#define GYM_PCB_VIA_H

#include "Py.hpp"
#include "Geometry.hpp"

class WideSegment_25;

class Via
{
public:
    Via() { }
    Via(const Point_2&, uint z1, uint z2, Real r);
    const Point_2& location() const { return mPoint; }
    uint zmin() const { return mLayer[0]; }
    uint zmax() const { return mLayer[1]; }
    uint z(uint i) const { return mLayer[i]; }
    uint height() const { return zmax() - zmin() + 1; }
    Real diameter() const { return mRadius * 2.0; }
    Real radius() const { return mRadius; }
    Real squared_radius() const { return mRadius * mRadius; }
    bool overlaps(const WideSegment_25&, Real clearance, Point_25 * = 0) const;
    bool overlaps(const Via&, Real clearance, Point_25 * = 0) const;
    bool contains(const Point_25&) const;
    Bbox_2 bbox() const;
    Circle_2 getCircle() const { return Circle_2(mPoint, mRadius * mRadius); }
    bool onLayer(uint z) const { return mLayer[0] <= z && z <= mLayer[1]; }
/// FIXME: We need to distinguish between the vertical path of a track and the via itself, which can extend over more layers.
    uint otherEnd(uint z) const;
    Point_25 otherEnd(const Point_25&) const;
    Point_25 zminEnd() const { return Point_25(mPoint, zmin()); }
    Point_25 zmaxEnd() const { return Point_25(mPoint, zmax()); }
    void extendTo(uint z);
    void merge(const Via&);
    uint cutLayers(uint zmin, uint zmax); // returns height left
    void setRange(uint zmin, uint zmax);
    static Via fromPy(PyObject *);
    std::string str() const;
    PyObject *getPy() const;
    PyObject *getShortPy() const;
private:
    Point_2 mPoint;
    uint mLayer[2];
    Real mRadius;
};
inline Via::Via(const Point_2 &v, uint z1, uint z2, Real r) : mPoint(v), mRadius(r)
{
    if (z1 == z2)
        throw std::invalid_argument("via of 0 height");
    mLayer[0] = std::min(z1, z2);
    mLayer[1] = std::max(z1, z2);
}
inline uint Via::otherEnd(uint z) const
{
    if (z != zmin() && z != zmax())
        throw std::runtime_error("the other end of this via from the specified point is not clearly defined");
    return z == zmin() ? zmax() : zmin();
}
inline Point_25 Via::otherEnd(const Point_25 &v) const
{
    assert(contains(v));
    return Point_25(v.xy(), otherEnd(v.z()));
}
inline void Via::extendTo(uint z)
{
    mLayer[0] = std::min(mLayer[0], z);
    mLayer[1] = std::max(mLayer[1], z);
}
inline void Via::setRange(uint zmin, uint zmax)
{
    assert(zmin <= zmax);
    mLayer[0] = zmin;
    mLayer[1] = zmax;
}

#endif // GYM_PCB_VIA_H
