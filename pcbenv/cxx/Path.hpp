
#ifndef GYM_PCB_PATH_H
#define GYM_PCB_PATH_H

#include "Py.hpp"
#include "Geometry.hpp"
#include <vector>

class Pin;
class Via;

/**
 * A continguous path (polyline) specified as a series of points in 2.5D space.
 * Unlike Track, Path has no width or vias.
 * Note that consecutive points must share either the z or the xy coordinates.
 */
class Path
{
public:
    const Point_25& getPoint(uint i) const { return mPoints.at(i); }
    const std::vector<Point_25>& getPoints() const { return mPoints; }
    void setPoint(uint i, const Point_25 &v) { mPoints.at(i) = v; }

    uint numPoints() const { return mPoints.size(); }
    bool empty() const { return mPoints.empty(); }

    const Point_25& front() const { return mPoints.front(); }
    const Point_25& back() const { return mPoints.back(); }
    const Point_25& back(uint n) const { assert(mPoints.size() >= n); return mPoints[mPoints.size() - n]; }
    Point_25 pop_back();

    /**
     * Return false if the path changes layer from point i-1 to point i.
     * @param i index > 0 and < numPoints
     * @throws std::out_of_range
     */
    bool isPlanarAt(uint i) const;

    /**
     * The segment from point i-1 to point i.
     * @param i index > 0 and < numPoints
     * @throws std::out_of_range
     */
    Segment_2 getSegmentTo(uint i) const;

    /**
     * The number of times the path changes layers.
     * Requires computeLength() to be called after updates to the path unless stated otherwise.
     */
    uint numLayerChanges() const { return mNumLayerChanges; }

    /**
     * The length of the path.
     * Requires computeLength() to be called after updates to the path unless stated otherwise.
     */
    Real length() const { return mLength; }

    /**
     * The root mean squared distance of the path from the straight line between the first and last point.
     * Requires computeRMSD() to be called after updates to the path.
     */
    Real RMSD() const { return mRMSD; }

    /**
     * The winding number is the sum of the signed angles between the segments (0 for an Z, 90 for an L, 180 for a U).
     * @return radians
     * Requires computeWinding() to be called after updates to the path.
     */
    float winding() const { return mWinding; }
    float turning() const { return mTurning; } //*< sum of absolute angles (in radius) between segments

    Bbox_2 bbox() const;
    Bbox_2 bbox(int z) const;

    /**
     * This does not perform any validity checks.
     */
    void setPoints(const std::vector<Point_25>&, uint start, uint size);

    void clear();

    /**
     * Compress the path by removing collinear segments if they continue in the same direction.
     * Also removes consecutive layer changes.
     * NOTE: This does not invalidate length.
     */
    uint removeUnnecessaryPoints();

    /**
     * Add a new point to the path and updates length and layer change count.
     * @param v must be either on the same layer or the same xy coordinates as back().
     */
    void add(const Point_25 &v);
    void _add(const Point_25 &v) { mPoints.push_back(v); }

    /**
     * Add a new point to the path and perform removeUnnecessaryPoints for it.
     * Unlike add(), this does not update length.
     */
    void extendTo(const Point_25&);

    /**
     * Appends the given path using extendTo() on its first point but does not optimize the remaining ones.
     */
    void append(const Path&);

    /**
     * Reverse the order of points in the path and negates the saved winding.
     */
    void reverse();

    void computeLength();
    void computeRMSD();
    void computeWinding(); //!< updates both winding and turning

    bool compare(const Path&) const; //!< check for exact match

    /**
     * Get the squared distance of the specified point from this path.
     * Iterates through all segments.
     * @param dz Distance added per difference in the z-coordinate.
     */
    Real squared_distance(const Point_25&, Real dz = std::numeric_limits<Real>::infinity()) const;
    Real squared_distance(const Pin&) const;

    /**
     * Check each segment against each other for intersection.
     * Takes into account layers.
     * @return 0 or 1 or, if count == true, the number of intersections found
     */
    uint intersects(const Path&, bool return_count = false) const;

    bool violatesClearance(const Path&, Real clearance) const;
    bool violatesClearance(const Via&, Real clearance) const;

    std::string str() const;
    PyObject *getIntegerPointsPy(Real scaleXY) const;
    PyObject *getPointsNumpy() const;
protected:
    std::vector<Point_25> mPoints;
    uint mNumLayerChanges{0};
    Real mLength{0.0f};
    Real mRMSD;
    float mWinding;
    float mTurning;
};

inline void Path::clear()
{
    mPoints.clear();
    mLength = 0.0f;
    mNumLayerChanges = 0;
}

inline bool Path::isPlanarAt(uint i) const
{
    return mPoints.at(i).z() == mPoints.at(i-1).z();
}
inline Segment_2 Path::getSegmentTo(uint i) const
{
    return Segment_2(mPoints.at(i-1).xy(), mPoints.at(i).xy());
}

inline Point_25 Path::pop_back()
{
    assert(!empty());
    const auto v = mPoints.back();
    mPoints.pop_back();
    return v;
}

#endif // GYM_PCB_PATH_H
