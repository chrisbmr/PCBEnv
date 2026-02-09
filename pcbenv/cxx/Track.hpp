#ifndef GYM_PCB_TRACK_H
#define GYM_PCB_TRACK_H

#include "AShape.hpp"
#include "Path.hpp"
#include "Via.hpp"

class NavGrid;
class Connection;

enum class ContactType
{
    NONE = 0,
    END_TO_START = 1,
    END_TO_END = 2,
    START_TO_END = 3,
    START_TO_START = 4
};

/**
 * Represents a contiguous track with wide segments and vias.
 * Segments and vias need only overlap rather than have coinciding endpoints and centers (but the latter is desirable).
 */
class Track
{
public:
    Track(const Point_25 &start) : mStart(start), mEnd(start) { }

    const Point_25& start() const { return mStart; }
    const Point_25& end() const { return mEnd; }

    bool empty() const { return mSegments.empty() && mVias.empty(); }

    const WideSegment_25& getSegment(uint i) const { return mSegments.at(i); }
    const std::vector<WideSegment_25>& getSegments() const { return mSegments; }
    uint numSegments() const { return mSegments.size(); }
    bool hasSegments() const { return !mSegments.empty(); }
    void setSegments(const std::vector<Segment_25>&);
    void setSegments(const std::vector<WideSegment_25>&);
    void setSegments(std::vector<WideSegment_25>&&);

    const Via& getVia(uint i) const { return mVias.at(i); }
    const std::vector<Via>& getVias() const { return mVias; }
    uint numVias() const { return mVias.size(); }
    bool hasVias() const { return !mVias.empty(); }
    bool startsOnViaCenter() const; //!< does not rely on start()
    bool startsOnVia() const; //!< relies on start()
    bool endsOnViaCenter() const; //!< does not rely on end()
    bool endsOnVia() const; //!< relies on end()

    ContactType canAttach(const Track&) const;
    bool touchesEndToStart(const Track&) const;
    bool touchesEndToEnd(const Track&) const;
    bool touchesStartToStart(const Track&) const;
    bool touchesStartToEnd(const Track&) const;

    void setStart(const Point_25&);
    void setEnd(const Point_25&);
    void _setEnd(const Point_25 &v);
    void _setEndLayer(uint);
    void _setEndLayer(const Point_25&);
    void moveStartTo(const Point_25&);
    void moveEndTo(const Point_25&);
    void inferStart();
    void inferEnd();
    void inferEndpoints();

    void setPath(const Path&);
    void getPath(Path&) const;
    void setVias(const std::vector<Via> &vias) { setModified(); mVias = vias; }
    void setVias(std::vector<Via> &&vias) { setModified(); mVias = std::move(vias); }
    void autocreateVias(const Point_25 &end);

    void append(const Track&);
    void appendMove(Track *);
    void append(const WideSegment_25&);
    void appendSegment(const Point_25&, const Point_25&);
    void append(const Via&);
    void appendVia(const Point_2&, uint z0, uint z1, Real r = std::numeric_limits<Real>::quiet_NaN());
    void prepend(const Via&);
    Via popVia();
    WideSegment_25 popSegment();
    void popSafe();
    void _append(const WideSegment_25&);
    void _append(const Segment_25&);
    void _appendVia(const Point_2&, uint z0, uint z1, Real r);

    void extendTo(const Point_25&, uint viaLocation);
    void _extendTo(const Point_25&);
    void extendToLayer(uint z);

    bool hasValidEnds() const; //!< check whether start and end variables correspond to segments and vias
    bool isContiguous() const; //!< check whether segment[i] target == segment[i+1].source

    void clear();

    Real reduceBy_cells(Real len_cells, const Real len_perLayer, const Real tolerance);
    Real reduceSegmentsBy_cells(Real len_cells, const Real tolerance);
    Real reduceViaBy_cells(Real len_cells, const Real len_perLayer, const Real tolerance);

    void reverse();
    static void _reverseSegments(std::vector<WideSegment_25>&);

    Real defaultWidth() const { return mWidth; }
    Real defaultViaDiameter() const { return mViaDiameter; }
    Real defaultViaRadius() const { return mViaDiameter * 0.5; }

    void setDefaultWidth(float w) { mWidth = w; }
    void setDefaultViaDiameter(Real d) { mViaDiameter = d; }

    /**
     * Length is maintained by all operations except _append().
     */
    Real length() const { return mLength; }

    void computeLength();

    void setSegmentCapsMask(uint8_t m) { mSegmentCapsMask = m; }
    bool hasStartCap() const { return mSegmentCapsMask & 0x1; }
    bool hasEndCap() const { return mSegmentCapsMask & 0x2; }
    bool hasSegmentJoints() const { return mSegmentCapsMask & 0x4; }

    /**
     * Return the bounding box over this track, either for a specific layer or all layers.
     *
     * @param clearance The box is expanded by this value with geo::expand_box_abs().
     * @param z Layer selection: < 0 means all layers.
     */
    Bbox_2 bbox(Real clearance, int z = -1) const;

    /**
     * Change the argument to either the start or the end of this track if it is within maxDistance.
     *
     * @return -1/+1 if snapped to start/end, or 0 if not within range
     */
    int snapToEndpoint(Point_25&, const Point_25&, Real maxDistance = 0.0) const;
    bool snapToStart(Point_25&, const Point_25&, Real maxDistance = 0.0) const;
    bool snapToEnd(Point_25&, const Point_25&, Real maxDistance = 0.0) const;

    /**
     * Insert the indices of the segments that touch other tracks of the specified connection.
     */
    void determineForks(std::vector<uint> &indices, const Connection&);

    /**
     * Check whether the other item comes within the specified clearance of this one.
     * @param x If non-null, set to the approximate location of the first clearance violation.
     * @return Whether a clearance violation was detected.
     */
    bool violatesClearance(const Pin&, Real clearance, Point_25 *x = 0) const;
    bool violatesClearance(const Track&, Real clearance, Point_25 * = 0) const;
    bool violatesClearance2D(const Track&, Real clearance, Point_25 * = 0) const;
    bool violatesClearance(const Via&, Real clearance, Point_25 * = 0) const;
    bool violatesClearance(const WideSegment_25&, Real clearance, Point_25 * = 0) const;

    /**
     * Check whether any segments or vias intersect the bounding box on the specified layer range.
     * Takes into account segment width.
     */
    bool intersects(const Bbox_2&, uint zmin, uint zmax) const;

    /**
     * The track maintains a grid rasterization count that is usually modified by rasterization calls.
     */
    bool isRasterized() const { return mRasterizedCount > 0; }
    void resetRasterizedCount() { mRasterizedCount = 0; }
    void addRasterizedCount(int count) const;
    /**
     * Check that isRasterized() and the keepout counts of the grid cells of the start and end points are > 0.
     */
    bool checkRasterization(const NavGrid&) const;

    /**
     * Update the z-coordinate values of the vias and segments.
     * If any segments or vias are removed completely the track becomes invalid.
     * @return whether the track is still valid
     */
    bool updateForRemovedLayers(uint zmin, uint zmax);

    void setModified() { mCacheDirty = true; } //!< invalidate cached bounding box

    std::string str() const;
    void setPy(PyObject *);
    bool _setSegmentsPyArray(PyObject *);
    void _setSegmentsPyList(PyObject *);
    bool _setViasPyArray(PyObject *);
    void _setViasPyList(PyObject *);
    PyObject *getViasPy() const;
    PyObject *getViasNumpy() const;
    PyObject *getSegmentsPy() const;
    PyObject *getSegmentsNumpy() const;
    PyObject *getPathPy() const;
    PyObject *getPathNumpy() const;
    PyObject *getPy(bool asNumpy) const;
private:
    std::vector<WideSegment_25> mSegments;
    std::vector<Via> mVias;
    Point_25 mStart;
    Point_25 mEnd;
    Real mWidth{0.0};
    Real mViaDiameter{0.0};
    Real mLength{0.0};
    mutable Bbox_2 mCachedBbox;
    mutable bool mCacheDirty{true};
    uint8_t mSegmentCapsMask{0x7}; // 0x4|0x2|0x1 for joints|track end|track start
private:
    mutable int mRasterizedCount{0};
};

inline bool Track::startsOnViaCenter() const
{
    if (!hasVias())
        return false;
    if (!hasSegments())
        return true;
    return mVias[0].location() == mSegments[0].source_2() && mVias[0].onLayer(mSegments[0].z());
}
inline bool Track::endsOnViaCenter() const
{
    if (!hasVias())
        return false;
    if (!hasSegments())
        return true;
    return mVias.back().location() == mSegments.back().target_2() && mVias.back().onLayer(mSegments.back().z());
}
inline bool Track::startsOnVia() const
{
    const bool rv = hasSegments() ? (mStart.z() != mSegments.front().z()) : hasVias();
    assert(!rv || (hasVias() && mVias.front().contains(mStart)));
    return rv;
}
inline bool Track::endsOnVia() const
{
    const bool rv = hasSegments() ? (mEnd.z() != mSegments.back().z()) : hasVias();
    assert(!rv || (hasVias() && mVias.back().contains(mEnd)));
    return rv;
}

inline Via Track::popVia()
{
    assert(endsOnVia());
    setModified();
    const auto via = mVias.back();
    mVias.pop_back();
    mEnd = hasSegments() ? mSegments.back().target() : mStart;
    return via;
}
inline WideSegment_25 Track::popSegment()
{
    assert(hasSegments() && !endsOnVia());
    setModified();
    const auto s = mSegments.back();
    mSegments.pop_back();
    inferEnd();
    mLength -= s.base().length();
    return s;
}
inline void Track::popSafe()
{
    if (endsOnVia())
        popVia();
    else if (hasSegments())
        popSegment();
}

inline void Track::appendMove(Track *T)
{
    append(*T);
    T->clear();
}

inline void Track::prepend(const Via &v)
{
    assert(v.contains(mStart));
    setModified();
    mVias.insert(mVias.begin(), v);
}

inline void Track::_setEnd(const Point_25 &v)
{
    mEnd = v;
}
inline void Track::_setEndLayer(uint z)
{
    mEnd = Point_25(mEnd.xy(), z);
}
inline void Track::_setEndLayer(const Point_25 &v)
{
    if (mEnd.xy() != v.xy())
        throw std::runtime_error("track end does not match specified endpoint");
    _setEndLayer(v.z());
}

inline void Track::appendSegment(const Point_25 &v0, const Point_25 &v1)
{
    if (v0.z() == v1.z())
        throw std::invalid_argument("segment points must be on the same layer");
    append(WideSegment_25(v0.xy(), v1.xy(), v0.z(), mWidth * 0.5));
}

inline void Track::_append(const WideSegment_25 &s)
{
    setModified();
    mSegments.emplace_back(s);
}
inline void Track::_append(const Segment_25 &s)
{
    setModified();
    mSegments.emplace_back(s, mWidth * 0.5);
}
inline void Track::_appendVia(const Point_2 &c, uint z0, uint z1, Real r)
{
    setModified();
    mVias.emplace_back(c, z0, z1, r);
}

#endif // GYM_PCB_TRACK_H
