#ifndef GYM_PCB_RASTERIZER_H
#define GYM_PCB_RASTERIZER_H

/// This is generic rasterization code that works for any class derived from UniformGrid25.
/// Transforms shapes to the set of grid indices they cover.
/// All you need to do is implement the rasterizatin operation and pass it as a template parameter to the rasterizer (for speed purposes).
/// You can also use RecordRangesROP implemented here to simply get the index ranges back.

#include "UniformGrid25.hpp"
#include "AShape.hpp"

#define RASTERIZE_CAPS_MASK_SOURCE 0x1
#define RASTERIZE_CAPS_MASK_TARGET 0x2

#define RASTERIZE_CAPS_MASK_BOTH (RASTERIZE_CAPS_MASK_SOURCE | RASTERIZE_CAPS_MASK_TARGET)

#define RASTERIZE_MASK_SEGMENTS  0x1
#define RASTERIZE_MASK_VIAS      0x2
#define RASTERIZE_MASK_CAPS      0x4
#define RASTERIZE_MASK_JUNCTIONS 0x8
#define RASTERIZE_MASK_ALL       0xf

#define RASTERIZE_MASK_CAPS_AND_JUNCTIONS (RASTERIZE_MASK_CAPS | RASTERIZE_MASK_JUNCTIONS)

class Track;

struct IndexRange
{
    IndexRange(uint z0, uint z1, uint y0, uint y1, uint x0, uint x1) : Z0(z0), Z1(z1), Y0(y0), Y1(y1), X0(x0), X1(x1) { }
    uint Z0, Z1;
    uint Y0, Y1;
    uint X0, X1;
    uint countX() const { return X1 - X0 + 1; }
    uint countY() const { return Y1 - Y0 + 1; }
    uint countXY() const { return countX() * countY(); }
    uint countZ() const { return Z1 - Z0 + 1; }
    uint count() const { return countXY() * countZ(); }
};

class BaseROP
{
public:
    void reserve(uint N) { }
    virtual void writeRangeZYX(uint Z0, uint Z1, uint Y0, uint Y1, uint X0, uint X1) = 0;
    void writeRangeZX(uint Z0, uint Z1, uint Y, uint X0, uint X1) { writeRangeZYX(Z0, Z1, Y, Y, X0, X1); }
    void writeRangeZY(uint Z0, uint Z1, uint Y0, uint Y1, uint X) { writeRangeZYX(Z0, Z1, Y0, Y1, X, X); }
    void writeRangeYX(uint Z, uint Y0, uint Y1, uint X0, uint X1) { writeRangeZYX(Z, Z, Y0, Y1, X0, X1); }
    void writeRangeY(uint Z, uint Y0, uint Y1, uint X) { writeRangeZYX(Z, Z, Y0, Y1, X, X); }
    void writeRangeX(uint Z, uint Y, uint X0, uint X1) { writeRangeZYX(Z, Z, Y, Y, X0, X1); }
};
class RecordRangesROP final : public BaseROP
{
public:
    void reserve(uint N) { mRanges.reserve(mRanges.size() + N); }
    void writeRangeZYX(uint Z0, uint Z1, uint Y0, uint Y1, uint X0, uint X1) override { mRanges.emplace_back(Z0, Z1, Y0, Y1, X0, X1); }
    const std::vector<IndexRange>& getRanges() const { return mRanges; }
private:
    std::vector<IndexRange> mRanges;
};

template<class ROP> class Rasterizer
{
public:
    Rasterizer(const UniformGrid25 &G) : mGrid(G) { mTolerance = G.EdgeLen / 1024.0; }
    ROP OP;

    void setExpansion(Real s) { mExpansion = s; }

    // Rules:
    // * All shapes except segments are closed sets (bundaries included).
    // * Fill uses the midpoint rule except when only 1 point is covered.
    // * Segments are open sets on the sides and closed sets on the caps.

    void rasterizeFill(const Bbox_2&, uint Z0, uint Z1);
    void rasterizeLine(const Bbox_2&, uint Z0, uint Z1);
    void rasterizeFill(const Segment_25&);
    void rasterizeLine(const Segment_25&);
    void rasterizeFill(const Segment_25&, uint Z0, uint Z1);
    void rasterizeLine(const Segment_25&, uint Z0, uint Z1);
    void rasterizeFill(const WideSegment_25&, uint capsMask = RASTERIZE_CAPS_MASK_BOTH);
    void rasterizeLine(const WideSegment_25&, uint capsMask = RASTERIZE_CAPS_MASK_BOTH);
    void rasterizeFill(const WideSegment_25&, uint capsMask, uint Z0, uint Z1);
    void rasterizeLine(const WideSegment_25&, uint capsMask, uint Z0, uint Z1);
    void rasterizeFill(const Circle_2&, uint Z0, uint Z1);
    void rasterizeLine(const Circle_2&, uint Z0, uint Z1);
    void rasterizeFill(const Triangle_2&, uint Z0, uint Z1);
    void rasterizeLine(const Triangle_2&, uint Z0, uint Z1);
    void rasterizeFill(const Polygon_2&, uint Z0, uint Z1);
    void rasterizeLine(const Polygon_2&, uint Z0, uint Z1);

    void rasterizeFill(const AShape *, uint Z0, uint Z1);
    void rasterizeLine(const AShape *, uint Z0, uint Z1);

    void rasterizeFill(const Track&, uint mask = RASTERIZE_MASK_ALL);
    void rasterizeLine(const Track&);

    void rasterizeRanges(const std::vector<IndexRange>&);

private:
    void rasterizeDSegment(const Segment_2&, uint Z0, uint Z1, Real ex);
    void rasterizeHSegment(const Segment_2&, uint Z0, uint Z1, Real ex);
    void rasterizeVSegment(const Segment_2&, uint Z0, uint Z1, Real ex);
    void rasterizeDSegmentLine(const Segment_2&, uint Z0, uint Z1);
    void rasterizeHSegmentLine(const Segment_2&, uint Z0, uint Z1);
    void rasterizeVSegmentLine(const Segment_2&, uint Z0, uint Z1);

private:
    const UniformGrid25 &mGrid;
    Real mExpansion{0.0};
    Real mTolerance;
};

#endif // GYM_PCB_RASTERIZER_H
