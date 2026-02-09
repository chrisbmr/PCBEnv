
#include "Log.hpp"
#include "PyArray.hpp"
#include "Track.hpp"
#include "Net.hpp"
#include "Connection.hpp"
#include "Layer.hpp"
#include "NavGrid.hpp"

void Track::clear()
{
    assert(mRasterizedCount == 0);
    setModified();
    if (mRasterizedCount > 0)
        WARN("Clearing track that is marked as rasterized!");
    mSegments.clear();
    mVias.clear();
    mEnd = mStart;
    mLength = 0.0;
}

void Track::setStart(const Point_25 &v)
{
    if (hasSegments()) {
        if (mSegments.front().source_2() != v.xy())
            throw std::runtime_error("start location for track does not match its path");
        if (mSegments.front().z() != v.z())
            if (!hasVias() || !mVias[0].contains(v))
                throw std::runtime_error("start layer for track does not match its path");
    } else if (hasVias() && !mVias[0].contains(v)) {
        throw std::runtime_error("start location for track does not match source via");
    }
    mStart = v;
}

void Track::setEnd(const Point_25 &v)
{
    if (hasSegments()) {
        if (mSegments.back().target_2() != v.xy())
            throw std::runtime_error("end location for track does not match its path");
        if (mSegments.back().z() != v.z())
            if (!hasVias() || !mVias.back().contains(v))
                throw std::runtime_error("end layer for track does not match its path");
    } else if (hasVias() && !mVias.back().contains(v)) {
        throw std::runtime_error("end location for track does not match final via");
    }
    mEnd = v;
}

void Track::moveStartTo(const Point_25 &v)
{
    if (!hasSegments())
        throw std::runtime_error("cannot move start point without segments");
    if (mSegments[0].z() != v.z())
        throw std::runtime_error("cannot move start point to different layer");
    setModified();
    mStart = v;
    mSegments[0]._setBase(Segment_25(v.xy(), mSegments[0].target_2(), v.z()));
}

void Track::moveEndTo(const Point_25 &v)
{
    if (!hasSegments())
        throw std::runtime_error("cannot move end point without segments");
    if (mSegments.back().z() != v.z())
        throw std::runtime_error("cannot move end point to different layer");
    setModified();
    mEnd = v;
    mSegments.back()._setBase(Segment_25(mSegments.back().source_2(), v.xy(), v.z()));
}

void Track::setSegments(const std::vector<Segment_25> &segs)
{
    if (!segs.empty() && mStart.xy() != segs.front().source_2())
        throw std::runtime_error("new segments don't match start point");
    if (hasVias())
        throw std::runtime_error("must not use setSegments() if vias are present");
    clear();
    for (const auto &s : segs) {
        mLength += s.length();
        mSegments.emplace_back(s, mWidth * 0.5);
    }
    if (!segs.empty())
        mEnd = segs.back().target();
}
void Track::setSegments(const std::vector<WideSegment_25> &segs)
{
    if (!segs.empty() && mStart.xy() != segs.front().source_2())
        throw std::runtime_error("new segments don't match start point");
    if (hasVias())
        throw std::runtime_error("must not use setSegments() if vias are present");
    clear();
    for (const auto &s : segs) {
        mLength += s.base().length();
        mSegments.emplace_back(s);
    }
    if (!segs.empty())
        mEnd = segs.back().target();
}
void Track::setSegments(std::vector<WideSegment_25> &&segs)
{
    if (!segs.empty() && mStart.xy() != segs.front().source_2())
        throw std::runtime_error("new segments don't match start point");
    if (hasVias())
        throw std::runtime_error("must not use setSegments() if vias are present");
    setModified();
    mSegments = std::move(segs);
    computeLength();
    mEnd = mSegments.back().target();
}

void Track::setPath(const Path &path)
{
    clear();
    if (mStart != path.front())
        throw std::runtime_error("path start does not match designated start of track");
    for (uint i = 1; i < path.numPoints(); ++i) {
        const auto &v0 = path.getPoint(i-1);
        const auto &v1 = path.getPoint(i);
        if (v0.z() == v1.z()) {
            assert(v0 != v1);
            _append(WideSegment_25(v0.xy(), v1.xy(), v0.z(), mWidth * 0.5));
            mLength += mSegments.back().base().length();
        } else {
            assert(v0.xy() == v1.xy());
            _appendVia(v0.xy(), v0.z(), v1.z(), defaultViaRadius());
        }
    }
    _setEnd(path.numPoints() ? path.back() : mStart);
}

void Track::getPath(Path &path) const
{
    path._add(mStart);
    for (const auto &s : mSegments) {
        if (s.z() != path.back().z())
            path._add(s.source());
        path._add(s.target());
    }
    if (path.back() != mEnd)
        path._add(mEnd);
}

void Track::computeLength()
{
    mLength = 0.0;
    for (const auto &s : mSegments)
        mLength += s.base().length();
}

void Track::append(const Track &T)
{
    if (T.empty())
        return;
    if (end() != T.start())
        throw std::runtime_error("tried to append a discontiguous track (consider creating a new connection)");
    setModified();
    int viaTouchIndex = -1;
    Via viaTouch;
    if (endsOnVia() && T.startsOnVia()) {
        viaTouchIndex = mVias.size() - 1;
        viaTouch = popVia();
    }
    _setEnd(T.end());
    mSegments.insert(mSegments.end(), T.mSegments.begin(), T.mSegments.end());
    mVias.insert(mVias.end(), T.mVias.begin(), T.mVias.end());
    mLength += T.mLength;
    if (viaTouchIndex >= 0)
        mVias[viaTouchIndex].merge(viaTouch);
}

void Track::append(const WideSegment_25 &s)
{
    if (end() != s.source())
        throw std::runtime_error("tried to append a discontiguous segment");
    _setEnd(s.target());
    _append(s);
    mLength += s.base().length();
}

void Track::append(const Via &via)
{
    appendVia(via.location(), via.zmin(), via.zmax(), via.radius());
}
void Track::appendVia(const Point_2 &c, uint z0, uint z1, Real r)
{
    if (std::isnan(r))
        r = defaultViaRadius();
    if (endsOnVia())
        throw std::runtime_error("tried to append a via to a track that already ends with a via");
    if (mEnd.xy() != c) {
        if (CGAL::squared_distance(mEnd.xy(), c) > math::squared(r))
            throw std::runtime_error("tried to append via at discontiguous location");
        WARN("appending via at inexact location");
    }
    _appendVia(c, z0, z1, r);
    _setEnd(mVias.back().otherEnd(mEnd));
}

void Track::extendToLayer(uint z)
{
    if (mEnd.z() == int(z))
        return;
    if (endsOnVia()) {
        mVias.back().extendTo(z);
    } else {
        _appendVia(mEnd.xy(), mEnd.z(), z, defaultViaRadius());
    }
    _setEndLayer(z);
}
void Track::_extendTo(const Point_25 &v)
{
    if (v == mEnd)
        return;
    if (mEnd.z() != v.z())
        throw std::runtime_error("cannot extend track to new point without a via");
    setModified();
    mLength += std::sqrt(CGAL::squared_distance(mEnd.xy(), v.xy()));
    if (mSegments.empty()) {
        _append(WideSegment_25(mStart.xy(), v.xy(), v.z(), mWidth * 0.5));
    } else {
        WideSegment_25 &s = mSegments.back();
        assert(s.target_2() == mEnd.xy());
        if (Ray_2(s.source_2(), s.target_2()).has_on(v.xy()))
            s = WideSegment_25(s.source_2(), v.xy(), s.z(), s.halfWidth());
        else
            _append(WideSegment_25(s.target_2(), v.xy(), v.z(), s.halfWidth()));
    }
    _setEnd(v);
}
void Track::extendTo(const Point_25 &v, uint viaLocation)
{
    if (viaLocation == 0)
        extendToLayer(v.z());
    _extendTo(viaLocation == 0 ? v : Point_25(v.xy(), mEnd.z()));
    if (viaLocation == 1)
        extendToLayer(v.z());
}

void Track::autocreateVias(const Point_25 &end)
{
    if (!mVias.empty())
        throw std::runtime_error("tried to create vias from segments when vias already exist");
    if (mSegments.empty()) {
        if (mStart.xy() != end.xy())
            throw std::runtime_error("tried to create via between differing xy coordinates");
        _appendVia(mStart.xy(), mStart.z(), end.z(), defaultViaRadius());
        _setEndLayer(end.z());
        return;
    }
    assert(mEnd.xy() == mSegments.back().target().xy());
    if (end.xy() != mEnd.xy())
        throw std::runtime_error("declared end point x,y location does not match the segments");
    if (mStart.z() != mSegments.front().z())
        _appendVia(mStart.xy(), mStart.z(), mSegments.front().z(), defaultViaRadius());
    auto z = mSegments.front().z();
    for (const auto &s : mSegments) {
        if (s.z() == z)
            continue;
        _appendVia(s.source_2(), z, s.z(), defaultViaRadius());
        z = s.z();
    }
    const auto zBack = mSegments.back().z();
    if (zBack != end.z()) {
        _appendVia(mEnd.xy(), zBack, end.z(), defaultViaRadius());
        _setEndLayer(end.z());
    }
}

bool Track::isContiguous() const
{
    for (uint i = 1; i < numSegments(); ++i)
        if (mSegments[i].source_2() != mSegments[i-1].target_2())
            return false;
    return true;
}

Real Track::reduceSegmentsBy_cells(Real len_cells, const Real tolerance)
{
    setModified();
    mLength = std::numeric_limits<Real>::quiet_NaN();
    const auto zStart = mEnd.z();
    while (!mSegments.empty()) {
        const auto &s = mSegments.back();
        if (s.z() != zStart)
            break;
        Real c = s.base().maxNorm();
        if (c > (len_cells + tolerance))
            break;
        len_cells -= c;
        mEnd = s.source();
        mSegments.pop_back();
    }
    if (mSegments.empty() || mSegments.back().z() != zStart || len_cells <= 0.0)
        return len_cells;
    const auto &s = mSegments.back().base();
    mEnd = Point_25(s.source_2() + s.s2().to_vector() * ((s.maxNorm() - len_cells) / s.maxNorm()), mEnd.z());
    mSegments.back()._setBase(Segment_25(s.source_2(), mEnd.xy(), s.z()));
    return 0.0;
}
Real Track::reduceViaBy_cells(Real len_cells, const Real len_perLayer, const Real tolerance)
{
    setModified();
    uint z0 = mEnd.z();
    uint z1 = mVias.back().otherEnd(mEnd).z();
    const int dz = (z0 > z1) ? -1 : 1;
    for (; z1 != z0 && len_cells > 0.0; z0 += dz)
        len_cells -= len_perLayer;
    if (z0 == z1)
        mVias.pop_back();
    else
        mVias.back().setRange(std::min(z0, z1), std::max(z0, z1));
    _setEndLayer(z0);
    return len_cells;
}
Real Track::reduceBy_cells(Real len_cells, const Real len_perLayer, const Real tolerance)
{
    while (len_cells > 0.0 && !empty()) {
        if (endsOnVia())
            len_cells = reduceViaBy_cells(len_cells, len_perLayer, tolerance);
        else if (hasSegments())
            len_cells = reduceSegmentsBy_cells(len_cells, tolerance);
    }
    return len_cells; // leftover
}

void Track::_reverseSegments(std::vector<WideSegment_25> &segs)
{
    std::vector<WideSegment_25> srev;
    srev.reserve(segs.size());
    for (uint i = segs.size(); i > 0; --i)
        srev.push_back(segs[i-1].opposite());
    segs = std::move(srev);
}
void Track::reverse()
{
    if (hasVias())
        std::reverse(mVias.begin(), mVias.end());
    if (hasSegments())
        _reverseSegments(mSegments);
    const auto start = mEnd;
    setEnd(mStart);
    setStart(start);
}

Bbox_2 Track::bbox(Real clearance, int z) const
{
    if (!mCacheDirty && z == -1 && clearance == 0.0)
        return mCachedBbox;
    Bbox_2 box;
    for (const auto &s : mSegments)
        if (z < 0 || s.z() == z)
            box += s.bbox();
    for (const auto &via : mVias)
        if (z < 0 || via.onLayer(z))
            box += via.bbox();
    if (z == -1) {
        mCachedBbox = box;
        mCacheDirty = false;
    }
    return geo::bbox_expanded_abs(box, clearance);
}

bool Track::violatesClearance(const WideSegment_25 &s2, Real clearance, Point_25 *x) const
{
    for (const auto &s1 : mSegments)
        if (s1.violatesClearance(s2, clearance, x))
            return true;
    for (const auto &via : mVias)
        if (via.overlaps(s2, clearance))
            return true;
    return false;
}
bool Track::violatesClearance(const Via &via, Real clearance, Point_25 *x) const
{
    for (const auto &s : mSegments)
        if (via.overlaps(s, clearance, x))
            return true;
    for (const auto &v : mVias)
        if (via.overlaps(v, clearance, x))
            return true;
    return false;
}
bool Track::violatesClearance(const Pin &P, Real clearance, Point_25 *x) const
{
    for (const auto &s : mSegments)
        if (P.violatesClearance(s, clearance, x))
            return true;
    for (const auto &v : mVias)
        if (P.violatesClearance(v, clearance, x))
            return true;
    return false;
}

bool Track::violatesClearance(const Track &that, Real clearance, Point_25 *x) const
{
    if (!CGAL::do_overlap(bbox(clearance), that.bbox(clearance)))
        return false;
    for (const auto &s : mSegments)
        if (that.violatesClearance(s, clearance, x))
            return true;
    for (const auto &v : mVias)
        if (that.violatesClearance(v, clearance, x))
            return true;
    return false;
}
bool Track::violatesClearance2D(const Track &that, Real clearance, Point_25 *x) const
{
    if (!CGAL::do_overlap(bbox(clearance), that.bbox(clearance)))
        return false;
    for (const auto &s1 : mSegments)
    for (const auto &s2 : that.mSegments)
        if (s1.violatesClearance2D(s2, clearance, x))
            return true;
    return false;
}

bool Track::intersects(const Bbox_2 &bbox, uint z0, uint z1) const
{
    for (const auto &s : mSegments)
        if (uint(s.z()) >= z0 && uint(s.z()) <= z1 && s.intersects(bbox))
            return true;
    for (const auto &V : mVias)
        if (V.zmin() <= z1 && V.zmax() >= z0 && CGAL::do_intersect(V.getCircle(), bbox))
            return true;
    return false;
}

void Track::addRasterizedCount(int count) const
{
    mRasterizedCount += count;
    if (mRasterizedCount < 0 || mRasterizedCount > 1)
        WARN("Track rasterization count is " << mRasterizedCount);
    assert(mRasterizedCount >= 0 && mRasterizedCount <= 1);
}

bool Track::updateForRemovedLayers(uint zmin, uint zmax)
{
    setModified();
    for (WideSegment_25 &s : mSegments) {
        uint z = s.z();
        if (!adjustIndexForCutLayers(z, zmin, zmax))
            return false;
        s._base().setLayer(z);
    }
    for (Via &via : mVias)
        if (via.cutLayers(zmin, zmax))
            return false;
    return true;
}

PyObject *Track::getViasPy() const
{
    auto py = PyList_New(mVias.size());
    if (!py)
        return 0;
    for (uint i = 0; i < mVias.size(); ++i)
        PyList_SetItem(py, i, mVias[i].getShortPy());
    return py;
}
PyObject *Track::getViasNumpy() const
{
    if (mVias.empty())
        return py::NPArray<float>(0, (npy_intp)0, 5).release();
    const uint N = mVias.size();
    auto V = static_cast<float *>(std::malloc(N * 5 * sizeof(float)));
    uint i = 0;
    for (const auto &v : mVias) {
        V[i++] = v.location().x();
        V[i++] = v.location().y();
        V[i++] = v.zmin();
        V[i++] = v.zmax();
        V[i++] = v.radius();
    }
    return py::NPArray<float>(V, N, 5).ownData().release();
}
PyObject *Track::getSegmentsPy() const
{
    auto py = PyList_New(mSegments.size());
    if (!py)
        return 0;
    for (uint i = 0; i < mSegments.size(); ++i)
        PyList_SetItem(py, i, mSegments[i].getShortPy());
    return py;
}
PyObject *Track::getSegmentsNumpy() const
{
    if (mSegments.empty())
        return py::NPArray<float>(0, (npy_intp)0, 6).release();
    const uint N = mSegments.size();
    auto S = static_cast<float *>(std::malloc(N * 6 * sizeof(float)));
    uint i = 0;
    for (const auto &s : mSegments) {
        S[i++] = s.source().x();
        S[i++] = s.source().y();
        S[i++] = s.target().x();
        S[i++] = s.target().y();
        S[i++] = s.z();
        S[i++] = s.width();
    }
    return py::NPArray<float>(S, N, 6).ownData().release();
}
PyObject *Track::getPathPy() const
{
    Path R;
    getPath(R);
    return R.getIntegerPointsPy(1.0);
}
PyObject *Track::getPathNumpy() const
{
    Path R;
    getPath(R);
    return R.getPointsNumpy();
}
PyObject *Track::getPy(bool asNumpy) const
{
    auto py = PyDict_New();
    if (!py)
        return 0;
    py::Dict_StealItemString(py, "start", *py::Object(mStart));
    py::Dict_StealItemString(py, "end", *py::Object(mEnd));
    py::Dict_StealItemString(py, "width", PyFloat_FromDouble(mWidth));
    py::Dict_StealItemString(py, "length", PyFloat_FromDouble(length()));
    py::Dict_StealItemString(py, "via_diam", PyFloat_FromDouble(mViaDiameter));
    py::Dict_StealItemString(py, "vias", asNumpy ? getViasNumpy() : getViasPy());
    py::Dict_StealItemString(py, "segments", asNumpy ? getSegmentsNumpy() : getSegmentsPy());
    if (true)
        py::Dict_StealItemString(py, "path", asNumpy ? getPathNumpy() : getPathPy());
    return py;
}

int Track::snapToEndpoint(Point_25 &rv, const Point_25 &v, Real maxDistance) const
{
    if (snapToEnd(rv, v, maxDistance))
        return +1;
    if (snapToStart(rv, v, maxDistance))
        return -1;
    return 0;
}
bool Track::snapToStart(Point_25 &rv, const Point_25 &v, Real maxDistance) const
{
    Real d = hasSegments() ? mSegments[0].halfWidth() : 0.0;
    if (startsOnVia())
        d = std::max(d, mVias[0].radius());
    if (CGAL::squared_distance(start().xy(), v.xy()) > math::squared(d + maxDistance))
        return false;
    rv = start();
    return true;
}
bool Track::snapToEnd(Point_25 &rv, const Point_25 &v, Real maxDistance) const
{
    Real d = hasSegments() ? mSegments.back().halfWidth() : 0.0;
    if (endsOnVia())
        d = std::max(d, mVias.back().radius());
    if (CGAL::squared_distance(end().xy(), v.xy()) > math::squared(d + maxDistance))
        return false;
    rv = end();
    return true;
}

void Track::inferEndpoints()
{
    inferStart();
    inferEnd();
}
void Track::inferStart()
{
    if (startsOnViaCenter()) {
        const Via &V = mVias.front();
        const uint z = hasSegments() ? mSegments.front().z() : V.zmax();
        mStart = Point_25(V.location(), V.otherEnd(z));
    } else {
        if (!hasSegments())
            throw std::runtime_error("cannot infer start of empty track");
        mStart = mSegments[0].source();
    }
}
void Track::inferEnd()
{
    if (endsOnViaCenter()) {
        const Via &V = mVias.back();
        const uint z = hasSegments() ? mSegments.back().z() : mStart.z();
        mEnd = Point_25(V.location(), V.otherEnd(z));
    } else if (hasSegments()) {
        mEnd = mSegments.back().target();
    } else {
        mEnd = mStart;
    }
}

bool Track::hasValidEnds() const
{
    if (empty())
        return mStart == mEnd;
    if (!hasVias())
        return hasSegments() &&
            mStart == mSegments.front().source() &&
            mEnd == mSegments.back().target();
    if (!hasSegments())
        return numVias() == 1 &&
            mStart.z() != mEnd.z() &&
            mVias[0].location() == mStart.xy() &&
            mVias[0].location() == mEnd.xy() &&
            mVias[0].onLayer(mStart.z()) &&
            mVias[0].onLayer(mEnd.z());
    return mStart.xy() == mSegments.front().source_2() &&
        mEnd.xy() == mSegments.back().target_2() &&
        (mStart.xy() == mVias.front().location() ?
         (mStart.z() != mSegments.front().z() &&
          mVias.front().onLayer(mStart.z()) &&
          mVias.front().onLayer(mSegments.front().z())) : (mStart.z() == mSegments.front().z()))
        &&
        (mEnd.xy() == mVias.back().location() ?
         (mEnd.z() != mSegments.back().z() &&
          mVias.back().onLayer(mEnd.z()) &&
          mVias.back().onLayer(mSegments.back().z())) : (mEnd.z() == mSegments.back().z()));
}

bool Track::touchesStartToStart(const Track &T) const
{
    if (mStart.xy() != T.mStart.xy())
        return false;
    if (mStart.z() == T.mStart.z())
        return true;
    return Layer::rangesOverlap(startsOnVia() ? mVias.front().zmin() : mStart.z(),
                                startsOnVia() ? mVias.front().zmax() : mStart.z(),
                                T.startsOnVia() ? T.mVias.front().zmin() : T.mStart.z(),
                                T.startsOnVia() ? T.mVias.front().zmax() : T.mStart.z());
}
bool Track::touchesStartToEnd(const Track &T) const
{
    if (mStart.xy() != T.mEnd.xy())
        return false;
    if (mStart.z() == T.mEnd.z())
        return true;
    return Layer::rangesOverlap(startsOnVia() ? mVias.front().zmin() : mStart.z(),
                                startsOnVia() ? mVias.front().zmax() : mStart.z(),
                                T.endsOnVia() ? T.mVias.back().zmin() : T.mEnd.z(),
                                T.endsOnVia() ? T.mVias.back().zmax() : T.mEnd.z());
}
bool Track::touchesEndToStart(const Track &T) const
{
    if (mEnd.xy() != T.mStart.xy())
        return false;
    if (mEnd.z() == T.mStart.z())
        return true;
    return Layer::rangesOverlap(endsOnVia() ? mVias.back().zmin() : mEnd.z(),
                                endsOnVia() ? mVias.back().zmax() : mEnd.z(),
                                T.startsOnVia() ? T.mVias.front().zmin() : T.mStart.z(),
                                T.startsOnVia() ? T.mVias.front().zmax() : T.mStart.z());
}
bool Track::touchesEndToEnd(const Track &T) const
{
    if (mEnd.xy() != T.mEnd.xy())
        return false;
    if (mEnd.z() == T.mEnd.z())
        return true;
    return Layer::rangesOverlap(endsOnVia() ? mVias.back().zmin() : mEnd.z(),
                                endsOnVia() ? mVias.back().zmax() : mEnd.z(),
                                T.endsOnVia() ? T.mVias.back().zmin() : T.mEnd.z(),
                                T.endsOnVia() ? T.mVias.back().zmax() : T.mEnd.z());
}
ContactType Track::canAttach(const Track &T) const
{
    assert(hasValidEnds() && T.hasValidEnds());
    if (touchesEndToStart(T))
        return ContactType::END_TO_START;
    if (touchesEndToEnd(T))
        return ContactType::END_TO_END;
    if (touchesStartToEnd(T))
        return ContactType::START_TO_END;
    if (touchesStartToStart(T))
        return ContactType::START_TO_START;
    return ContactType::NONE;
}

bool Track::_setSegmentsPyArray(PyObject *py)
{
    FloatNPArray A(py);
    if (!A.valid())
        return false;
    A.INCREF();
    if (!((A.degree() == 2 && A.dim(1) == 6) ||
          (A.degree() == 1 && !(A.count() % 6))))
        throw std::invalid_argument("segments array must have size (n,6) or (n*6)");
    if (!A.isPacked())
        throw std::invalid_argument("segments array must be packed");
    const float *v = A.data();
    for (uint i = 0; i < A.count(); i += 6)
        mSegments.push_back(WideSegment_25(Point_2(v[i], v[i+1]), Point_2(v[i+2], v[i+3]), v[i+4], v[i+5]));
    return true;
}
void Track::_setSegmentsPyList(PyObject *py)
{
    py::Object segs(py);
    const uint numSegs = segs.listSize("track.segs must be a list or np.array of float32");
    mSegments.reserve(numSegs);
    mLength = 0.0;
    for (uint i = 0; i < numSegs; mLength += mSegments[i].base().length(), ++i)
        mSegments.push_back(WideSegment_25::fromPy(*segs.item(i)));    
}
bool Track::_setViasPyArray(PyObject *py)
{
    FloatNPArray A(py);
    if (!A.valid())
        return false;
    A.INCREF();
    if (!((A.degree() == 2 && A.dim(1) == 5) ||
          (A.degree() == 1 && !(A.count() % 5))))
        throw std::invalid_argument("vias array must have size (n,5) or (n*5)");
    if (!A.isPacked())
        throw std::invalid_argument("vias array must be packed");
    const float *v = A.data();
    for (uint i = 0; i < A.count(); i += 6)
        mVias.push_back(Via(Point_2(v[i], v[i+1]), v[i+2], v[i+3], v[i+4]));
    return true;
}
void Track::_setViasPyList(PyObject *py)
{
    py::Object vias(py);
    const uint numVias = vias.listSize("track.vias must be a list or np.array of float32");
    mVias.reserve(numVias);
    for (uint i = 0; i < numVias; ++i)
        mVias.push_back(Via::fromPy(*vias.item(i)));
}

void Track::setPy(PyObject *py)
{
    clear();
    py::Object args(py);
    if (!args)
        return;

    auto segs = args.item("segments");
    auto vias = args.item("vias");
    if (segs)
        if (!_setSegmentsPyArray(*segs))
            _setSegmentsPyList(*segs);
    if (vias)
        if (!_setViasPyArray(*vias))
            _setViasPyList(*vias);
    inferEndpoints();

    auto width = args.item("width");
    auto via_diam = args.item("via_diam");
    if (width)
        mWidth = width.toDouble();
    if (via_diam)
        mViaDiameter = via_diam.toDouble();
}

std::string Track::str() const
{
    if (empty())
        return "{}";
    std::stringstream ss;
    uint v = 0;
    ss << "START" << mStart;
    for (const auto &s : getSegments()) {
        for (; v < mVias.size() && mVias[v].location() == s.source_2(); ++v)
            ss << " O(" << mVias[v].zmin() << ',' << mVias[v].zmax() << ')';
        ss << " S" << s.target();
    }
    for (; v < mVias.size(); ++v)
        ss << " O(" << mVias[v].zmin() << ',' << mVias[v].zmax() << ')';
    ss << " END" << mEnd;
    return ss.str();
}

bool Track::checkRasterization(const NavGrid &nav) const
{
    if (!isRasterized())
        return false;
    if (nav.getPoint(mStart)->getKOCounts()._RouteTracks < 1)
        return false;
    if (nav.getPoint(mEnd)->getKOCounts()._RouteTracks < 1)
        return false;
    return true;
}

void Track::determineForks(std::vector<uint> &indices, const Connection &X0)
{
    if (!X0.net())
        return;
    for (uint i = 0; i < mSegments.size(); ++i) {
        const auto &s1 = mSegments[i];
        for (const auto X : X0.net()->connections()) {
            if (X == &X0)
                continue;
            bool touch = false;
            for (const auto *T : X->getTracks()) {
                for (const auto &s2 : T->getSegments()) {
                    touch = s1.intersects(s2);
                    if (touch)
                        break;
                }
                if (touch)
                    break;
                for (const auto &v : T->getVias()) {
                    touch = v.onLayer(s1.z()) && s1.intersects(v.getCircle());
                    if (touch)
                        break;
                }
            }
            if (touch)
                indices.push_back(i);
        }
    }
}
