#ifndef GYM_PCB_NAVGRID_H
#define GYM_PCB_NAVGRID_H

#include "NavPoint.hpp"
#include "Rasterizer.hpp"
#include "Rules.hpp"

// FIXME: 90°-track endpoints may be brushed on by 45°-tracks. Write testcase!

class PCBoard;
class Component;
class Pin;
class Path;
class Connection;

/**
 * We cannot safely erase individual segments without messing up overlap regions if
 * rasterizing a track only increases the KO counts by 1 for each NavPoint it covers
 * instead of by N when N segments/joints cover the same NavPoint.
 */
constexpr const bool CanSafelyEraseOverlappingSegments = false;

/**
 * This struct stores the spacing requirements the NavGrid is/should be prepared for.
 */
struct NavSpacings
{
    Real Clearance; /**< clearance required by the net to be routed */
    Real TrackWidthHalf;
    Real ViaRadius;
    NavSpacings() { }
    NavSpacings(const DesignRules &R) : Clearance(R.Clearance), TrackWidthHalf(R.TraceWidth * 0.5), ViaRadius(R.viaRadius()) { }
    NavSpacings(const Connection&);
    bool operator==(const NavSpacings &that) const;
    /**
     * @param clearance The clearance requirement for the object to be rasterized (not the net to be routed).
     * @return how much to expand the object to be rasterized for the current net
     */
    Real getExpansionForTracks(Real clearance) const;
    Real getExpansionForVias(Real clarance) const;
};
inline bool NavSpacings::operator==(const NavSpacings &that) const
{
    return Clearance == that.Clearance && TrackWidthHalf == that.TrackWidthHalf && ViaRadius == that.ViaRadius;
}

struct AStarCosts
{
    float MaskedLayer;
    float Via;
    float Violation;
    float TurnPer45Degrees;
    float WrongDirection;
    std::string PreferredDirections;
    void reset();
    void setViolationCostInf() { Violation = std::numeric_limits<float>::infinity(); }
    bool valid() const { return MaskedLayer >= 0.0f && Via >= 0.0f && Violation >= 0.0f && WrongDirection >= 0.0f; }
    void setPy(PyObject *);
};

/**
 * This is the main 3D "navigation grid" for A* where grid-based local routing happens.
 * Grid cells are represented by NavPoints owned by the NavGrid class.
 * The grid is always prepared to route tracks of a certain clearance, width, and via size.
 * During A*, we search for a path for the mid-line/center of a track/via.
 * Instead of checking a whole area around each point, we expand all obstacles such that
 *  a point is only free if tracks/vias centered on it wouldn't violate any constraints.
 * Thus, for a width of 4 units and a clearance of 1 unit, all obstacles are expanded by 3:
 * Clearance + width / 2 (width is halved because we go from the track center to its edge).
 * Note that the clearances of 2 objects don't add, we just need to maintain the larger one.
 */
class NavGrid : public UniformGrid25
{
public:
    NavGrid(PCBoard&);
    const PCBoard& getPCB() const { return mPCB; }
    PCBoard& getPCB() { return mPCB; }

    void build();
    void copyFrom(const NavGrid&);

    const std::vector<NavPoint>& getPoints() const { return mPoints; }
    std::vector<NavPoint>& getPoints() { return mPoints; }
    uint getNumPoints() const { return mPoints.size(); }

    const NavPoint& getPoint(uint i) const { return mPoints[i]; }
    const NavPoint& getPoint(uint x, uint y, uint z) const;
    const NavPoint *getPoint(const Point_2&, uint z) const;
    const NavPoint *getPoint(const Point_25 &v) const { return getPoint(v.xy(), v.z()); }
    NavPoint& getPoint(uint i) { return mPoints[i]; }
    NavPoint& getPoint(uint x, uint y, uint z);
    NavPoint& getPoint(const IPoint_3 &v) { return getPoint(v.x, v.y, v.z); }
    NavPoint *getPoint(const Point_2&, uint z);
    NavPoint *getPoint(const Point_25 &v) { return getPoint(v.xy(), v.z()); }

    const NavSpacings& getSpacings() const { return mSpacings; }
    bool setSpacings(const NavSpacings&);
    void initSpacingsForAnyRoutedTrack();

    void resetKeepouts();
    void resetUserKeepout(uint index);
    void resetUserKeepouts();

    void rasterize(const Component&, const NavRasterizeParams&);
    void rasterize(const Pin&, const NavRasterizeParams&);
    void rasterizeCompound(const Pin&, const NavRasterizeParams&);
    void rasterize(const Connection&, const NavRasterizeParams&);
    void rasterizeLayoutAreaBorder(const NavRasterizeParams&);
    void rasterize(const Track&, const NavRasterizeParams&, uint rasterizeMask);
    void rasterize(const Path&, const NavRasterizeParams&);

    void getCosts(std::vector<float>&);
    void setCosts(float);
    void setCosts(const float *, float base);
    void setCosts(const IBox_3&, float);
    void setCosts(const IBox_3&, const float *, float base);

    AStarCosts& getAStarCosts() { return mAStarCosts; }
    bool findPathAStar(Connection&, const AStarCosts *);

    Real sumViolationArea(const Connection&);

    uint16_t nextRasterSeq();
    uint16_t nextSearchSeq();
    uint16_t getSearchSeq() const { return mSearchSeq; }

    std::string str(const IBox_3 * = 0) const;
    PyObject *getPy(const IBox_3&) const;
    PyObject *getPathCoordinatesNumpy(const Track&) const;

    int getDirectionStride(GridDirection d) const { assert(d.n() <= 9); return mDirectionStride[d.n()]; }

private:
    PCBoard &mPCB;
    std::vector<NavPoint> mPoints;
    NavSpacings mSpacings;
    int mDirectionStride[10]; /**< We use these to look up the addresses of neighbours in the grid because NavPoint doesn't have edge pointers (to save space). */
    AStarCosts mAStarCosts;
    uint16_t mSearchSeq{0}; /**< To mark nodes already visited during an instance of A-star. */
    uint16_t mRasterSeq{0}; /**< To mark nodes already written during a rasterization pass. */

private:
    void initDirectionStrides();
    void initEdges(NavPoint&, const IPoint_3&);
    void rasterizeFootprints();
    void rasterizeClearanceAreas();
    void rasterize(const AShape *, uint Z0, uint Z1, const NavRasterizeParams&);
    void resetSearchSeq();
    void resetRasterSeq();
};

inline NavPoint& NavGrid::getPoint(uint x, uint y, uint z)
{
    return mPoints.at(LinearIndex(z,y,x));
}
inline const NavPoint& NavGrid::getPoint(uint x, uint y, uint z) const
{
    return mPoints.at(LinearIndex(z,y,x));
}
inline NavPoint *NavGrid::getPoint(const Point_2 &v, uint z)
{
    const uint x = XIndex(v.x(), 0.0);
    const uint y = YIndex(v.y(), 0.0);
    return inside(x,y,z) ? &getPoint(x, y, z) : 0;
}
inline const NavPoint *NavGrid::getPoint(const Point_2 &v, uint z) const
{
    const uint x = XIndex(v.x(), 0.0);
    const uint y = YIndex(v.y(), 0.0);
    return inside(x,y,z) ? &getPoint(x, y, z) : 0;
}

inline uint16_t NavGrid::nextSearchSeq()
{
    if (mSearchSeq == 0x7fff) // 0x8000 is used to indicate that a point is on A-star's open list
        resetSearchSeq();
    return ++mSearchSeq;
}
inline uint16_t NavGrid::nextRasterSeq()
{
    if (mRasterSeq == 0xffff)
        resetRasterSeq();
    return ++mRasterSeq;
}

inline Real NavSpacings::getExpansionForTracks(Real clearance) const
{
    return std::max(Clearance, clearance) + TrackWidthHalf;
}
inline Real NavSpacings::getExpansionForVias(Real clearance) const
{
    return std::max(Clearance, clearance) + ViaRadius;
}

inline NavPoint *NavPoint::getEdge(NavGrid &nav, GridDirection d) const
{
    char *_this = const_cast<char *>(reinterpret_cast<const char *>(this));
    if (!hasEdge(d))
        return 0;
    return reinterpret_cast<NavPoint *>(_this + nav.getDirectionStride(d));
}

#endif // GYM_PCB_NAVGRID_H
