
#include "PyArray.hpp"
#include "Log.hpp"
#include "Path.hpp"
#include "Math/Misc.hpp"
#include "Pin.hpp"
#include <algorithm>
#include <sstream>

Bbox_2 Path::bbox() const
{
    Bbox_2 box;
    for (const auto &v : mPoints)
        box += v.bbox();
    return box;
}
Bbox_2 Path::bbox(int z) const
{
    Bbox_2 box;
    for (const auto &v : mPoints)
        if (v.z() == z)
            box += v.bbox();
    return box;
}

void Path::setPoints(const std::vector<Point_25> &v, uint start, uint size)
{
    clear();
    mPoints.assign(&v[start], &v[start + size]);
}

void Path::add(const Point_25 &v)
{
    if (!empty()) {
        if (v == back())
            return;
        if (back().z() != v.z()) {
            if (back().xy() != v.xy())
                throw std::runtime_error("layer changes must not include planar movement");
            mNumLayerChanges++;
        } else {
            mLength += std::sqrt(CGAL::squared_distance(back().xy(), v.xy()));
        }
    }
    _add(v);
}

void Path::extendTo(const Point_25 &v)
{
    if (numPoints() >= 2 && v.z() == back().z() && v.z() == back(1).z() && Ray_2(back(1).xy(), back().xy()).has_on(v.xy()))
        mPoints.back() = v;
    else
    if (numPoints() >= 2 && v.z() != back().z() && v.z() != back(1).z() && (v.z() < back().z()) == (back().z() < back(1).z()))
        mPoints.back() = v;
    else
        add(v);
}

uint Path::removeUnnecessaryPoints()
{
    uint n = 1;
    for (uint i = 2; i < mPoints.size(); ++i) {
        const auto &v0 = mPoints[i].xy();
        const uint  z0 = mPoints[i].z();
        const auto &v1 = mPoints[i-1].xy();
        const uint  z1 = mPoints[i-1].z();
        const auto &v2 = mPoints[i-2].xy();
        const uint  z2 = mPoints[i-2].z();
        if (z0 == z1 && z1 == z2 && Ray_2(v2, v1).has_on(v0)) {
            mPoints[n] = mPoints[i];
        } else if (z0 != z1 && z1 != z2) {
            if (v0 != v1 || v1 != v2)
                throw std::runtime_error("illegal path detected");
            mPoints[n] = mPoints[i];
        } else {
            ++n;
        }
    }
    const uint r = mPoints.size() - n;
    mPoints.resize(n);
    return r;
}

void Path::reverse()
{
    if (!mPoints.empty())
        std::reverse(mPoints.begin(), mPoints.end());
    mWinding = -mWinding;
}

void Path::append(const Path &other)
{
    if (other.numPoints() == 1)
        throw std::runtime_error("appending a path with 1 point is undefined");
    if (other.numPoints() < 2)
        return;
    extendTo(other.front());
    mPoints.insert(mPoints.end(), ++other.mPoints.begin(), other.mPoints.end());
}

void Path::computeLength()
{
    mNumLayerChanges = 0;
    mLength = 0.0f;
    for (uint i = 1; i < mPoints.size(); ++i) {
        if (!isPlanarAt(i))
            ++mNumLayerChanges;
        else
            mLength += std::sqrt(CGAL::squared_distance(mPoints[i-1].xy(), mPoints[i].xy()));
    }
}

void Path::computeRMSD()
{
    mRMSD = 0.0;
    if (numPoints() < 3)
        return;
    const Point_2 A = front().xy();
    const Point_2 B = back().xy();
    const auto dAB = std::sqrt(CGAL::squared_distance(A, B));
    if (dAB == 0.0)
        return;
    const Vector_2 Vn = (B - A) / dAB;
    const Line_2 L(A, B);
    Real h0 = 0.0;
    Real d = 0.0;
    for (uint i = 1; i < mPoints.size(); ++i) {
        if (mPoints[i].z() != mPoints[i-1].z())
            continue;
        const auto sv = mPoints[i].xy() - mPoints[i-1].xy();
        // Project the length of the segment onto the line and multiply that by the average height of the two endpoints to get the area between segment and line.
        auto dL = std::abs(CGAL::scalar_product(Vn, sv));
        Real h1 = std::sqrt(CGAL::squared_distance(mPoints[i].xy(), L));
        mRMSD += (h0 + h1) * 0.5 * dL;
        d += dL;
        h0 = h1;
    }
    mRMSD /= d;
}

void Path::computeWinding()
{
    mWinding = 0.0f;
    mTurning = 0.0f;
    if (numPoints() < 3)
        return;
    // i/k/m are the last 3 points that differ from each other in the 2D plane
    uint m = 1;
    uint i = 0;
    uint k = 0;
    while (mPoints[m].z() != mPoints[m-1].z())
        ++m;
    for (uint n = m + 1; n < mPoints.size(); ++n) {
        if (mPoints[n].xy() == mPoints[n-1].xy())
            continue;
        i = k;
        k = m;
        m = n;
        const auto s0 = Segment_25(mPoints[i].xy(), mPoints[k].xy());
        const auto s1 = Segment_25(mPoints[k].xy(), mPoints[m].xy());
        const auto A = s0.angleWith(s1);
        mWinding += A;
        mTurning += std::abs(A);
    }
}

bool Path::compare(const Path &that) const
{
    if (numPoints() != that.numPoints())
        return false;
    for (uint i = 0; i < mPoints.size(); ++i)
        if (mPoints[i] != that.mPoints[i])
            return false;
    return true;
}

Real Path::squared_distance(const Point_25 &v, Real dz) const
{
    const Real dz2 = dz * dz;
    Real d = std::numeric_limits<float>::infinity();
    for (uint i = 1; (d > 0.0) && (i < mPoints.size()); ++i)
        if (isPlanarAt(i))
            d = std::min(d, CGAL::squared_distance(getSegmentTo(i), v.xy()) + math::squared(v.z() - mPoints[i].z()) * dz2);
    return d;
}

Real Path::squared_distance(const Pin &T) const
{
    Real d = std::numeric_limits<float>::infinity();
    for (uint i = 1; (d > 0.0) && (i < mPoints.size()); ++i)
        if (isPlanarAt(i) && T.isOnLayer(mPoints[i].z()))
            d = std::min(d, T.getShape()->squared_distance(getSegmentTo(i)));
    return d;
}

uint Path::intersects(const Path &that, bool return_count) const
{
    if (!CGAL::do_overlap(bbox(), that.bbox()))
        return 0;
    uint count = 0;
    for (uint i = 1; i < mPoints.size(); ++i) {
        if (!isPlanarAt(i))
            continue;
        const auto s_i = getSegmentTo(i);
        for (uint k = 1; k < that.mPoints.size(); ++k) {
            if (that.isPlanarAt(k) && mPoints[i].z() == mPoints[k].z()) {
                if (CGAL::do_intersect(s_i, that.getSegmentTo(k))) {
                    if (!return_count)
                        return 1;
                    count += 1;
                }
            }
        }
    }
    return count;
}

std::string Path::str() const
{
    std::stringstream ss;
    ss << '{';
    for (const auto &v : mPoints)
        ss << ' ' << v;
    ss << " | l=" << mLength << " dz=" << mNumLayerChanges << " }";
    return ss.str();
}

PyObject *Path::getIntegerPointsPy(Real scale) const
{
    if (mPoints.empty())
        return PyList_New(0);
    auto py = PyList_New(mPoints.size());
    for (uint i = 0; i < mPoints.size(); ++i)
        PyList_SetItem(py, i, *py::Object::new_TupleOfLongs(mPoints[i], 1.0));
    return py;
}
PyObject *Path::getPointsNumpy() const
{
    if (mPoints.empty())
        return py::NPArray<float>(0, (npy_intp)0, 3).release();
    const uint N = mPoints.size();
    auto V = static_cast<float *>(std::malloc(N * 3 * sizeof(float)));
    uint i = 0;
    for (const auto &v : mPoints) {
        V[i++] = v.x();
        V[i++] = v.y();
        V[i++] = v.z();
    }
    return py::NPArray<float>(V, N, 3).ownData().release();
}
