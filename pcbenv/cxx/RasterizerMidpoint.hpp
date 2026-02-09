
#include "Log.hpp"
#include "Rasterizer.hpp"
#include "Track.hpp"

template<class ROP> void Rasterizer<ROP>::rasterizeFill(const Bbox_2 &box, uint Z0, uint Z1)
{
    assert(Z1 >= Z0 && Z1 < mGrid.getSize(2));
    if (Z1 < Z0 || Z1 >= mGrid.getSize(2))
        return;
    const auto ex = mExpansion - mGrid.EdgeLen05 - mTolerance;
    const auto eh = std::max(ex, box.xmin() - box.xmax());
    const auto ev = std::max(ex, box.ymin() - box.ymax());
    const auto X0 = mGrid.XIndexBounded(box.xmin(), -eh);
    const auto X1 = mGrid.XIndexBounded(box.xmax(), +eh);
    const auto Y0 = mGrid.YIndexBounded(box.ymin(), -ev);
    const auto Y1 = mGrid.YIndexBounded(box.ymax(), +ev);
    OP.writeRangeZYX(Z0, Z1, Y0, Y1, X0, X1);
}

template<class ROP> void Rasterizer<ROP>::rasterizeLine(const Bbox_2 &box, uint Z0, uint Z1)
{
    assert(Z1 >= Z0 && Z1 < mGrid.getSize(2));
    if (Z1 < Z0 || Z1 >= mGrid.getSize(2))
        return;
    const auto ex = mExpansion - mGrid.EdgeLen05 - mTolerance;
    const auto eh = std::max(ex, box.xmin() - box.xmax());
    const auto ev = std::max(ex, box.ymin() - box.ymax());
    const auto X0 = mGrid.XIndexBounded(box.xmin(), -eh);
    const auto X1 = mGrid.XIndexBounded(box.xmax(), +eh);
    const auto Y0 = mGrid.YIndexBounded(box.ymin(), -ev);
    const auto Y1 = mGrid.YIndexBounded(box.ymax(), +ev);
    OP.reserve(4);
    OP.writeRangeZX(Z0, Z1, Y0, X0, X1);
    if (Y1 == Y0)
        return;
    OP.writeRangeZX(Z0, Z1, Y1, X0, X1);
    if (Y1 == (Y0 + 1))
        return;
    OP.writeRangeZY(Z0, Z1, Y0+1, Y1-1, X0);
    if (X0 != X1)
        OP.writeRangeZY(Z0, Z1, Y0+1, Y1-1, X1);
}

template<class ROP> void Rasterizer<ROP>::rasterizeFill(const Circle_2 &C, uint Z0, uint Z1)
{
    assert(Z1 >= Z0 && Z1 < mGrid.getSize(2));
    const auto &O = C.center();
    const auto r  = std::sqrt(C.squared_radius()) + mExpansion;
    const auto r2 = r * r;
    const auto ex = std::min(mGrid.EdgeLen05 + mTolerance, r); // ensure Y0 <= Y1
    const auto Y0 = mGrid.YIndexBounded(O.y() - r, +ex);
    const auto Y1 = mGrid.YIndexBounded(O.y() + r, -ex);
    DEBUG("rasterizeCircle: O=" << O << " r=" << r << " Z=[" << Z0 << ',' << Z1 << "] Y=[" << Y0 << ',' << Y1 << ']');
    auto cY = mGrid.MidPointY(Y0) - O.y(); // y-distance from circle center to midpoint of lowest row
    assert(Y0 <= Y1);
    assert(cY <= mGrid.EdgeLen05); // can be > 0 for small circles centered below the midpoint
    OP.reserve(Y1 - Y0 + 1);
    for (auto Y = Y0; Y <= Y1; ++Y, cY += mGrid.EdgeLen) {
        const auto cX = std::sqrt(std::max(r2 - cY * cY, 0.0)); // x-distance corresponding to y-distance on circle
        const auto X0 = mGrid.XIndexBounded(O.x() - cX, +ex);
        const auto X1 = mGrid.XIndexBounded(O.x() + cX, -ex);
        OP.writeRangeZX(Z0, Z1, Y, X0, X1);
    }
}

template<class ROP> void Rasterizer<ROP>::rasterizeLine(const Circle_2&, uint Z0, uint Z1)
{
    throw std::runtime_error("FIXME: circle border rasterization");
}

template<class ROP> void Rasterizer<ROP>::rasterizeFill(const WideSegment_25 &s, uint capsMask)
{
    rasterizeFill(s, capsMask, s.z(), s.z());
}
template<class ROP> void Rasterizer<ROP>::rasterizeFill(const WideSegment_25 &s, uint capsMask, uint Z0, uint Z1)
{
    assert(Z0 >= 0 && Z0 <= Z1 && Z1 < mGrid.getSize(2));

    if (capsMask & RASTERIZE_CAPS_MASK_SOURCE)
        rasterizeFill(s.sourceCap(), Z0, Z1);
    if (capsMask & RASTERIZE_CAPS_MASK_TARGET)
        rasterizeFill(s.targetCap(), Z0, Z1);

    const Real ex = s.halfWidth() + mExpansion;
    if (s.base().is_horizontal())
        rasterizeHSegment(s.base().s2(), Z0, Z1, ex);
    else if (s.base().is_vertical())
        rasterizeVSegment(s.base().s2(), Z0, Z1, ex);
    else rasterizeDSegment(s.base().s2(), Z0, Z1, ex);
}

template<class ROP> void Rasterizer<ROP>::rasterizeLine(const WideSegment_25 &s, uint capsMask)
{
    rasterizeLine(capsMask ? Segment_25(s.s2_extended(capsMask), s.z()) : s.base());
}
template<class ROP> void Rasterizer<ROP>::rasterizeLine(const WideSegment_25 &s, uint capsMask, uint Z0, uint Z1)
{
    rasterizeLine(capsMask ? Segment_25(s.s2_extended(capsMask), s.z()) : s.base(), Z0, Z1);
}

template<class ROP> void Rasterizer<ROP>::rasterizeFill(const Segment_25 &s)
{
    rasterizeFill(s, s.z(), s.z());
}
template<class ROP> void Rasterizer<ROP>::rasterizeFill(const Segment_25 &s, uint Z0, uint Z1)
{
    assert(Z0 >= 0 && Z0 <= Z1 && Z1 < mGrid.getSize(2));
    const Real ex = mExpansion;
    if (s.is_horizontal())
        rasterizeHSegment(s.s2(), Z0, Z1, ex);
    else if (s.is_vertical())
        rasterizeVSegment(s.s2(), Z0, Z1, ex);
    else rasterizeDSegment(s.s2(), Z0, Z1, ex);
}

template<class ROP> void Rasterizer<ROP>::rasterizeLine(const Segment_25 &s)
{
    rasterizeLine(s, s.z(), s.z());
}
template<class ROP> void Rasterizer<ROP>::rasterizeLine(const Segment_25 &s, uint Z0, uint Z1)
{
    assert(Z0 >= 0 && Z0 <= Z1 && Z1 < mGrid.getSize(2));
    if (s.is_horizontal())
        rasterizeHSegmentLine(s.s2(), Z0, Z1);
    else if (s.is_vertical())
        rasterizeVSegmentLine(s.s2(), Z0, Z1);
    else rasterizeDSegmentLine(s.s2(), Z0, Z1);
}

template<class ROP> void Rasterizer<ROP>::rasterizeFill(const Triangle_2 &T, uint Z0, uint Z1)
{
    assert(Z1 >= Z0 && Z1 < mGrid.getSize(2));
    if (mExpansion)
        throw std::runtime_error("FIXME: triangle rasterization with dilation");
    const auto ex = mGrid.EdgeLen05 + mTolerance;
    const auto box = T.bbox();
    const auto XL = mGrid.XIndexBounded(box.xmin(), +ex);
    const auto XR = mGrid.XIndexBounded(box.xmax(), -ex);
    const auto xL = mGrid.MidPointX(XL);
    const auto xR = mGrid.MidPointX(XR);
    const auto Y0 = mGrid.YIndexBounded(box.ymin(), +ex);
    const auto Y1 = mGrid.YIndexBounded(box.ymax(), -ex);
    const auto y0 = mGrid.MidPointY(Y0);
    OP.reserve(Y1 - Y0 + 1);
    Point_2 v(xL, y0);
    for (auto Y = Y0; Y <= Y1; ++Y, v = Point_2(xL, v.y() + mGrid.EdgeLen)) {
        uint X0, X1;
        for (X0 = XL, v = Point_2(xL, v.y()); X0 <= XR && T.has_on_unbounded_side(v); ++X0)
            v = Point_2(v.x() + mGrid.EdgeLen, v.y());
        if (X0 > XR)
            continue;
        for (X1 = XR, v = Point_2(xR, v.y()); X1 >  X0 && T.has_on_unbounded_side(v); --X1)
            v = Point_2(v.x() - mGrid.EdgeLen, v.y());
        OP.writeRangeZX(Z0, Z1, Y, X0, X1);
    }
}

template<class ROP> void Rasterizer<ROP>::rasterizeLine(const Triangle_2 &T, uint Z0, uint Z1)
{
    assert(Z1 >= Z0 && Z1 < mGrid.getSize(2));
    const auto ex = mExpansion;
    mExpansion = 0.0;
    rasterizeLine(Segment_2(T.vertex(0), T.vertex(1)), Z0, Z1);
    rasterizeLine(Segment_2(T.vertex(1), T.vertex(2)), Z0, Z1);
    rasterizeLine(Segment_2(T.vertex(2), T.vertex(0)), Z0, Z1);
    mExpansion = ex;
}

template<class ROP> void Rasterizer<ROP>::rasterizeFill(const Polygon_2 &_G, uint Z0, uint Z1)
{
    Polygon_2 G_ex;
    assert(Z1 >= Z0 && Z1 < mGrid.getSize(2));
    if (mExpansion)
        G_ex = PolygonEx::grow(_G, mExpansion);
    const Polygon_2 *G = mExpansion ? &G_ex : &_G;
    const auto ex = mGrid.EdgeLen05 + mTolerance;
    const auto box = G->bbox();
    const auto XL = mGrid.XIndexBounded(box.xmin(), +ex);
    const auto XR = mGrid.XIndexBounded(box.xmax(), -ex);
    const auto xL = mGrid.MidPointX(XL);
    const auto xR = mGrid.MidPointX(XR);
    const auto Y0 = mGrid.YIndexBounded(box.ymin(), +ex);
    const auto Y1 = mGrid.YIndexBounded(box.ymax(), -ex);
    const auto y0 = mGrid.MidPointY(Y0);
    OP.reserve(Y1 - Y0 + 1);
    Point_2 v(xL, y0);
    for (auto Y = Y0; Y <= Y1; ++Y, v = Point_2(xL, v.y() + mGrid.EdgeLen)) {
        uint X0, X1;
        for (X0 = XL, v = Point_2(xL, v.y()); X0 <= XR && G->has_on_unbounded_side(v); ++X0)
            v = Point_2(v.x() + mGrid.EdgeLen, v.y());
        if (X0 > XR)
            continue;
        for (X1 = XR, v = Point_2(xR, v.y()); X1 >  X0 && G->has_on_unbounded_side(v); --X1)
            v = Point_2(v.x() - mGrid.EdgeLen, v.y());
        OP.writeRangeZX(Z0, Z1, Y, X0, X1);
    }
}

template<class ROP> void Rasterizer<ROP>::rasterizeLine(const Polygon_2 &G, uint Z0, uint Z1)
{
    assert(Z1 >= Z0 && Z1 < mGrid.getSize(2));
    for (auto E = G.edges_begin(); E != G.edges_end(); ++E)
        rasterizeLine(*E, Z0, Z1);
}

template<class ROP> void Rasterizer<ROP>::rasterizeHSegment(const Segment_2 &s, uint Z0, uint Z1, Real _ex)
{
    // Midpoint rasterization:
    // 0.5 to 1.5 with shifting by 0.5 becomes 1 to 1 but we want to draw both 0 and 1 so we subtract tolerance on the left.
    // 0.5 to 1.4 becomes 1-TOL to 0.9 so we correctly only draw on 0.
    // 0.0 to 0.4 becomes 0.2-TOL to 0.2, but we draw on 0 anyway because we always draw at least 1 cell.
    // We also need to add tolerance on the right because there can be tiny errors such that we get 0 to 0.99999999999994 which should really be 1 with exact calculations.
    // Something will always be off somewhere :(
    assert(s.source().y() == s.target().y());
    const auto x0 = std::min(s.source().x(), s.target().x());
    const auto x1 = std::max(s.source().x(), s.target().x());
    const auto hex = std::min(mGrid.EdgeLen05 - mTolerance, (x1 - x0) * 0.5);
    const auto vex = std::min(mGrid.EdgeLen05 + mTolerance - _ex, 0.0);
    const auto X0 = mGrid.XIndexBounded(x0, +hex);
    const auto X1 = mGrid.XIndexBounded(x1, -hex);
    const auto Y0 = mGrid.YIndexBounded(s.source().y(), +vex);
    const auto Y1 = mGrid.YIndexBounded(s.source().y(), -vex);
    DEBUG("rasterizeSegmentH rect[" << X0 << ',' << X1 << "]x[" << Y0 << ',' << Y1 << "] x=(" << (x0 + hex - mTolerance) << ',' << (x1 - hex) << ')');
    OP.writeRangeZYX(Z0, Z1, Y0, Y1, X0, X1);
}

template<class ROP> void Rasterizer<ROP>::rasterizeHSegmentLine(const Segment_2 &s, uint Z0, uint Z1)
{
    rasterizeHSegment(s, Z0, Z1, 0.0);
}

template<class ROP> void Rasterizer<ROP>::rasterizeVSegment(const Segment_2 &s, uint Z0, uint Z1, Real _ex)
{
    assert(s.source().x() == s.target().x());
    const auto y0 = std::min(s.source().y(), s.target().y());
    const auto y1 = std::max(s.source().y(), s.target().y());
    const auto vex = std::min(mGrid.EdgeLen05 - mTolerance, (y1 - y0) * 0.5);
    const auto hex = std::min(mGrid.EdgeLen05 + mTolerance - _ex, 0.0);
    const auto X0 = mGrid.XIndexBounded(s.source().x(), +hex);
    const auto X1 = mGrid.XIndexBounded(s.source().x(), -hex);
    const auto Y0 = mGrid.YIndexBounded(y0, +vex);
    const auto Y1 = mGrid.YIndexBounded(y1, -vex);
    DEBUG("rasterizeSegmentV rect[" << X0 << ',' << X1 << "]x[" << Y0 << ',' << Y1 << "] y=(" << (y0 + vex - mTolerance) << ',' << (y1 - vex) << ')');
    OP.writeRangeZYX(Z0, Z1, Y0, Y1, X0, X1);
}

template<class ROP> void Rasterizer<ROP>::rasterizeVSegmentLine(const Segment_2 &s, uint Z0, uint Z1)
{
    rasterizeVSegment(s, Z0, Z1, 0.0);
}

template<class ROP> void Rasterizer<ROP>::rasterizeDSegment(const Segment_2 &_s, uint Z0, uint Z1, Real _ex)
{
    // TODO: Verify rasterization rules.
    auto s = WideSegment_25(_s, Z0, _ex);
    if (s.widerThanBaseLen())
        s = s.swapWL(mTolerance, -2.0 * mTolerance); // we have to increase the width a bit or we miss the start and end points of route segments
    s = s.orderedY();
    if (std::abs(s.base().s2().to_vector().y()) < 0x1.0p-7)
        throw std::runtime_error("FIXME: rasterizing nearly horizontal segment requires special handling");
    const auto ex = mGrid.EdgeLen05;
    const auto uT = s.getHalfWidthSpan();
    const auto vT = uT.x() < 0.0 ? -uT : uT; // perpendicular (half-width) vector with dx >= 0
    const auto sL = Segment_2(s.source_2() - vT, s.target_2() - vT); // left track boundary
    const auto sR = Segment_2(s.source_2() + vT, s.target_2() + vT); // right track boundary
    const auto LL = sL.supporting_line();
    const auto LR = sR.supporting_line();
    // Create a triangulation of the segment. sL and sR have the same endpoint order so the edges fit.
    const auto AL = Triangle_2(sL.source(), sL.target(), sR.source());
    const auto AR = Triangle_2(sR.source(), sR.target(), sL.target());
    const auto y0 = std::min(sL.source().y(), sR.source().y());
    const auto y1 = std::max(sL.target().y(), sR.target().y());
    const auto yA0 = std::max(sL.source().y(), sR.source().y()); // y-range that is delimited by LL and LR
    const auto yA1 = std::min(sL.target().y(), sR.target().y());
    const auto Y0 = mGrid.YIndexBounded(y0, +ex - mTolerance);
    const auto Y1 = mGrid.YIndexBounded(y1, -ex);
    auto y = mGrid.MidPointY(Y0);
    OP.reserve(Y1 - Y0 + 1);
    for (auto Y = Y0; Y <= Y1; ++Y, y += mGrid.EdgeLen) {
        int X0 = mGrid.XIndexBounded(LL.x_at_y(y), +ex - mTolerance);
        int X1 = mGrid.XIndexBounded(LR.x_at_y(y), -ex);
        if (y < yA0 || y > yA1) {
            auto m = Point_2(mGrid.MidPointX(X0), y);
            for (; X0 < X1 && AL.has_on_unbounded_side(m) && AR.has_on_unbounded_side(m); ++X0)
                m = Point_2(m.x() + mGrid.EdgeLen, m.y());
            if (X0 != X1)
                m = Point_2(mGrid.MidPointX(X1), m.y());
            for (; X1 >= X0 && AL.has_on_unbounded_side(m) && AR.has_on_unbounded_side(m); --X1)
                m = Point_2(m.x() - mGrid.EdgeLen, m.y());
        }
        if (X0 <= X1)
            OP.writeRangeZX(Z0, Z1, Y, X0, X1);
    }
}

template<class ROP> void Rasterizer<ROP>::rasterizeDSegmentLine(const Segment_2 &s, uint Z0, uint Z1)
{
    const auto ex = mTolerance + mGrid.EdgeLen05;
    const auto x0 = mGrid.XfIndex(std::min(s.source().x(), s.target().x()), +ex);
    const auto y0 = mGrid.YfIndex(std::min(s.source().y(), s.target().y()), +ex);
    const auto x1 = std::max(mGrid.XfIndex(std::max(s.source().x(), s.target().x()), -ex), x0); // == if within tolerance
    const auto y1 = std::max(mGrid.YfIndex(std::max(s.source().y(), s.target().y()), -ex), y0);
    const int  xR = std::min(int(mGrid.getSize(0)) - 1, int(x1)); // don't overshoot ...
    const int  xL = std::max(0, int(x0)); // or undershoot on nearly horizontal lines
    if (xL > xR)
        return;
    OP.reserve(int(y1 - y0 + 1));
    const auto xrev = (s.source().x() > s.target().x()) ^ (s.source().y() > s.target().y());
    const Real dxdy = (s.target().x() - s.source().x()) / (s.target().y() - s.source().y());
    Real x = xrev ? x1 : x0;
    for (auto y = y0; y <= y1; y += 1.0, x += dxdy) {
        const auto Y = int(y);
        if (Y < 0 || Y >= int(mGrid.getSize(1)))
            continue;
        auto X0 = std::max(xL, std::min(xR, int(x)));
        auto X1 = std::max(xL, std::min(xR, int(x + dxdy)));
        OP.writeRangeZX(Z0, Z1, Y, std::min(X0, X1), std::max(X0, X1));
    }
}

template<class ROP> void Rasterizer<ROP>::rasterizeFill(const AShape *H, uint Z0, uint Z1)
{
    assert(H);
    if (const auto R = dynamic_cast<const Iso_rectangle_2 *>(H))
        rasterizeFill(R->bbox(), Z0, Z1);
    else if (const auto C = dynamic_cast<const Circle_2 *>(H))
        rasterizeFill(*C, Z0, Z1);
    else if (const auto T = dynamic_cast<const Triangle_2 *>(H))
        rasterizeFill(*T, Z0, Z1);
    else if (const auto G = dynamic_cast<const Polygon_2 *>(H))
        rasterizeFill(*G, Z0, Z1);
    else if (const auto S = dynamic_cast<const WideSegment_25 *>(H))
        rasterizeFill(*S, 0x3, Z0, Z1);
    else if (H)
        rasterizeFill(H->bbox(), Z0, Z1);
}
template<class ROP> void Rasterizer<ROP>::rasterizeLine(const AShape *H, uint Z0, uint Z1)
{
    assert(H);
    if (const auto R = dynamic_cast<const Iso_rectangle_2 *>(H))
        rasterizeLine(R->bbox(), Z0, Z1);
    else if (const auto C = dynamic_cast<const Circle_2 *>(H))
        rasterizeLine(*C, Z0, Z1);
    else if (const auto T = dynamic_cast<const Triangle_2 *>(H))
        rasterizeLine(*T, Z0, Z1);
    else if (const auto G = dynamic_cast<const Polygon_2 *>(H))
        rasterizeLine(*G, Z0, Z1);
    else if (const auto S = dynamic_cast<const WideSegment_25 *>(H))
        rasterizeLine(*S, 0x3, Z0, Z1);
    else if (H)
        rasterizeLine(H->bbox(), Z0, Z1);
}

template<class ROP> void Rasterizer<ROP>::rasterizeFill(const Track &T, uint itemsMask)
{
    if (itemsMask & RASTERIZE_MASK_VIAS)
        for (const auto &v : T.getVias())
            rasterizeFill(v.getCircle(), v.zmin(), v.zmax());
    if (!T.hasSegments() || !(itemsMask & RASTERIZE_MASK_SEGMENTS))
        return;
    if (!(itemsMask & RASTERIZE_MASK_CAPS_AND_JUNCTIONS) || !T.hasSegmentJoints()) { // it's simpler if we don't have to draw any caps
        for (const auto &s : T.getSegments())
            rasterizeFill(s, 0x0);
        return;
    }
    uint mask = (T.hasStartCap() && !T.startsOnVia()) ? 0x0 : 0x2;
    for (uint i = 0; i < T.numSegments() - 1; ++i) {
        const auto &s = T.getSegment(i);
        const auto &t = T.getSegment(i+1);
        mask = (mask & 0x2) ? 0x0 : 0x1;
        // If the next segment is narrower and there is no via, rasterize the target cap and skip the next source cap.
        if (s.halfWidth() > t.halfWidth() && s.z() == t.z())
            mask |= 0x2;
        rasterizeFill(s, mask);
        if (s.z() != t.z())
            mask |= 0x2; // s ends in a via, so skip the next source cap
    }
    // Last segment.
    mask = (mask & 0x2) ? 0x0 : 0x1;
    if (T.hasEndCap() && !T.endsOnVia())
        mask |= 0x2;
    rasterizeFill(T.getSegments().back(), mask);
}
template<class ROP> void Rasterizer<ROP>::rasterizeLine(const Track &T)
{
    for (const auto &s : T.getSegments())
        rasterizeLine(s.base());
}

template<class ROP> void Rasterizer<ROP>::rasterizeRanges(const std::vector<IndexRange> &ranges)
{
    for (const IndexRange &R : ranges)
        for (uint Z = R.Z0; Z <= R.Z1; ++Z)
            for (uint Y = R.Y0; Y <= R.Y1; ++Y)
                for (uint I = mGrid.LinearIndex(Z,Y,R.X0), I1 = I + (R.X1 - R.X0); I <= I1; ++I)
                    OP.writeIndex(I);
}
