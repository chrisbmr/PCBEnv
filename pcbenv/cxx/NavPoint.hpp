#ifndef GYM_PCB_NAVPOINT_H
#define GYM_PCB_NAVPOINT_H

#include "Geometry.hpp"
#include "GridDirection.hpp"
#include "UniformGrid25.hpp"

/// Higher-level NavGrid:
/// Turns out this is not often useful for A* because when connections are dense it will only encounter free high-level tiles when nothing is routed.

class NavGrid;

/// Grid cell flags.
#define NAV_POINT_FLAG_BLOCKED_TEMPORARY     (1 << 0) // blockages other than pins or tracks
#define NAV_POINT_FLAG_BLOCKED_PERMANENT     (1 << 8) // holes etc.
#define NAV_POINT_FLAG_NO_VIAS               (1 << 1) // blockages other than pins or tracks affecting vias only
#define NAV_POINT_FLAG_INSIDE_PIN            (1 << 2) // cells covered by a pin
#define NAV_POINT_FLAG_INSIDE_COMPONENT      (1 << 3) // cells inside a component footprint
#define NAV_POINT_FLAG_PIN_TRACK_CLEARANCE   (1 << 4) // keepout area for tracks stemming from pins
#define NAV_POINT_FLAG_PIN_VIA_CLEARANCE     (1 << 5) // keepout area for vias stemming from pins
#define NAV_POINT_FLAG_ROUTE_TRACK_CLEARANCE (1 << 6) // keepout area for tracks stemming from other tracks
#define NAV_POINT_FLAG_ROUTE_VIA_CLEARANCE   (1 << 7) // keepout area for vias stemming from other tracks
#define NAV_POINT_FLAG_SOURCE                (1 << 9) // for use by A*
#define NAV_POINT_FLAG_TARGET                (1 << 10) // for use by A*
#define NAV_POINT_FLAG_ROUTE_GUARD           (1 << 11) // to restrict routing area

/// Combinations of the above flags (logical OR)
#define NAV_POINT_FLAGS_PIN_CLEARANCE   0x030 // PIN_TRACK|VIA_CLEARANCE
#define NAV_POINT_FLAGS_ROUTE_CLEARANCE 0x0c0 // ROUTE_TRACK|VIA_CEARANCE
#define NAV_POINT_FLAGS_TRACK_CLEARANCE 0x050 // PIN|ROUTE_TRACK_CLEARANCE
#define NAV_POINT_FLAGS_VIA_CLEARANCE   0x0a0 // PIN|ROUTE_VIA_CLEARANCE
#define NAV_POINT_FLAGS_CLEARANCE       0x0f0 // *_CLEARANCE
#define NAV_POINT_FLAGS_TRACKS_BLOCKED  0x951 // BLOCKED|TRACK_CLEARANCE|GUARD
#define NAV_POINT_FLAGS_VIAS_BLOCKED    0x9f3 // BLOCKED|NO_VIAS|CLEARANCE|GUARD
#define NAV_POINT_FLAGS_VIAS_BLOCKED_P  0x9f2 // BLOCKED_PERMANENT|NO_VIAS|CLEARANCE|GUARD
#define NAV_POINT_FLAGS_ENDPOINT        0x600 // SOURCE|TARGET
#define NAV_POINT_FLAGS_ALL             0xfff

/// This class is used to keep track of whether a NavPoint has been visited in the current invocation of A*.
/// We use a sequence number so we don't have to reset all visited nodes each time (until the sequence wraps).
class AStarVisitStatus
{
public:
    void reset() { mSeq = 0; }
    void setOpen(uint16_t seq) { mSeq = seq; }
    void setDone(uint16_t seq) { assert(isOpen(seq)); mSeq = seq | 0x8000; }
    bool isSeen(uint16_t seq) const { return (mSeq & 0x7fff) == seq; }
    bool isOpen(uint16_t seq) const { return mSeq == seq; }
    bool isDone(uint16_t seq) const { return mSeq == (seq | 0x8000); }
private:
    uint16_t mSeq{0};
};

/**
 * This struct keeps track of how many routes and pins require a grid cell to be kept free.
 * We need a count, as the clearance areas of multiple tracks and pins may overlap.
 * Sometimes, parts of multiple tracks or pins themselves may even pass through the same cell.
 * When we delete one source of keepout, this ensures the cell is still marked as occupied.
 */
struct NavKeepoutCounts
{
    void setPin(int8_t n)         { _RouteTracks = 0; _RouteVias = 0; _PinTracks = n; _PinVias = n; }
    void setPin(int8_t n, int8_t m) { _RouteTracks = 0; _RouteVias = 0; _PinTracks = n; _PinVias = m; }
    void setPinTracks(int8_t n)   { _RouteTracks = 0; _RouteVias = 0; _PinTracks = n; _PinVias = 0; }
    void setPinVias(int8_t n)     { _RouteTracks = 0; _RouteVias = 0; _PinTracks = 0; _PinVias = n; }
    void setRoute(int8_t n)       { _RouteTracks = n; _RouteVias = n; _PinTracks = 0; _PinVias = 0; }
    void setRoute(int8_t n, int8_t m) { _RouteTracks = n; _RouteVias = m; _PinTracks = 0; _PinVias = 0; }
    void setRouteTracks(int8_t n) { _RouteTracks = n; _RouteVias = 0; _PinTracks = 0; _PinVias = 0; }
    void setRouteVias(int8_t n)   { _RouteTracks = 0; _RouteVias = n; _PinTracks = 0; _PinVias = 0; }
    void reset()                  { _RouteTracks = 0; _RouteVias = 0; _PinTracks = 0; _PinVias = 0; }
    bool isZero() const { return _all == 0; }
    union {
        struct {
            int8_t _RouteTracks;
            int8_t _RouteVias;
            int8_t _PinTracks;
            int8_t _PinVias;
            int16_t _User[2];
        };
        uint64_t _all{0};
    };
};

/**
 * This struct contains parameters for updating the NavGrid's costs and flags when rasterizing shapes.
 * Note that the CLEARANCE flags are updated even if the KO counts are 0.
 */
struct NavRasterizeParams
{
    Real Expansion{0.0}; //!< Expand shapes outward by this amount (ignored with AutoExpand).
    uint16_t IgnoreMask{0}; //!< Skip all operations for points with hasFlags(IgnoreMask). */
    uint16_t FlagsAnd{0xffff}; //!< Clear flags not set here (may remove CLEARANCE).
    uint16_t FlagsOr{0}; //!< Set these flags (after FlagsAnd).
    mutable uint16_t WriteSeq; //!< Used to prevent writing overlapping points of track segments/joints twice.
    bool AutoExpand{false}; //!< Whether to rasterize the track & via clearance areas (assumes KOCount.Tracks != 0).
    bool AutoIncrementWriteSeq{true}; //!< If true, get a new WriteSeq from the grid before rasterization.
    NavKeepoutCounts KOCount; //!< Added to NavPoint's counts to determine CLEARANCE flags.
    int8_t TrackRasterCount{0}; //!< Added to Track's rasterization counter.

    bool isNoop() const;
};
inline bool NavRasterizeParams::isNoop() const
{
    return (FlagsAnd == 0xffff) && (FlagsOr == 0x0) && KOCount.isZero();
}

/**
 * This class represents a cell in the 3D grid representation of the board.
 * Grid cells will be 1x1 length units as per PCBoard(UnitLengthInNanoMeters).
 * Each cell has 8 horizontal + 2 vertical edges (indexed by GridDirection), pointing to its neighbours (or null).
 * NavPoint also contains temporary data required by A*.
 */
class NavPoint
{
public:
    void setRefPoint(int x, int y, int z) { mRefX = x; mRefY = y; mLayer = z; assert(x >= 0 && x <= 32767); assert(y >= 0 && y <= 32767); assert(z >= 0 && z <= 65535); }

    Point_2 getRefPoint(const UniformGrid25 *nav) const { return nav->MidPoint(mRefX, mRefY); }
    Point_25 getRefPoint25(const UniformGrid25 *nav) const { return Point_25(getRefPoint(nav), mLayer); }
    uint8_t getLayer() const { return mLayer; }

    bool hasEdge(GridDirection d) const { return mEdgeMask & (1 << d.n()); }
    NavPoint *getEdge(NavGrid&, GridDirection d) const;
    void setEdge(GridDirection d, bool mask);

    Segment_25 getSegmentTo(const NavPoint &v, const UniformGrid25 *nav) const { return Segment_25(getRefPoint(nav), v.getRefPoint(nav), mLayer); }

    const NavKeepoutCounts& getKOCounts() const { return mKOCount; }
    NavKeepoutCounts& getKOCounts() { return mKOCount; }
    void resetKO() { mKOCount.reset(); clearFlags(NAV_POINT_FLAGS_CLEARANCE); }
    bool canRoute() const { return !(mFlags & NAV_POINT_FLAGS_TRACKS_BLOCKED); }
    bool canPlaceVia() const { return !(mFlags & NAV_POINT_FLAGS_VIAS_BLOCKED); }
    bool canPlaceViaEver() const { return !(mFlags & NAV_POINT_FLAGS_VIAS_BLOCKED_P); }
    bool canAddVia(const NavPoint &to) const;
    bool canAddViaEver(const NavPoint &to) const;
    bool insidePin() const { return (mFlags & NAV_POINT_FLAG_INSIDE_PIN); }
    bool hasFlags(uint16_t mask) const { return !!(mFlags & mask); }
    uint16_t getFlags() const { return mFlags; }
    void setFlags(uint16_t mask) { mFlags |= mask; }
    void clearFlags(uint16_t mask) { mFlags &= ~mask; }
    void saveFlags() { mFlagsSaved = mFlags; }
    void restoreFlags() { mFlags = mFlagsSaved; }

    void write(const NavRasterizeParams&);
    void copyFrom(const NavPoint&);

    float getCost() const { return mCost; }
    void setCost(float cost) { mCost = cost; }
    float getScore() const { return mScore; }
    void setScore(float score) { mScore = score; }

    NavPoint *getBackEdge(NavGrid &nav) const { return getEdge(nav, mBackDir); }
    GridDirection getBackDirection() const { return mBackDir; }
    void setBackDirection(GridDirection d) { mBackDir = d; }

    int x() const { return mRefX; }
    int y() const { return mRefY; }
    int z() const { return mLayer; }

    AStarVisitStatus& getVisits() { return mVisits; }
    const AStarVisitStatus& getVisits() const { return mVisits; }

    uint16_t getWriteSeq() const { return mWriteSeq; }
    void setWriteSeq(uint16_t v) { mWriteSeq = v; }

    std::string str(const NavGrid *) const;
private:
    int16_t mRefX;              // 0
    int16_t mRefY;              // 2
    float mCost{1.0f};          // 4
    float mScore;               // 8
    uint16_t mFlags{0};         // 12
    uint16_t mFlagsSaved{0};    // 14
    uint16_t mEdgeMask{0};      // 16
    uint8_t mLayer;             // 18
    GridDirection mBackDir;     // 19
    AStarVisitStatus mVisits;   // 20
    NavKeepoutCounts mKOCount;  // 22
    uint16_t mWriteSeq{0};      // 30
    // align                    // 32
};

inline bool NavPoint::canAddVia(const NavPoint &to) const
{
    return canPlaceVia() && to.canPlaceVia() && !((mFlags ^ to.mFlags) & NAV_POINT_FLAG_INSIDE_PIN);
}
inline bool NavPoint::canAddViaEver(const NavPoint &to) const
{
    return canPlaceViaEver() && to.canPlaceViaEver() && !((mFlags ^ to.mFlags) & NAV_POINT_FLAG_INSIDE_PIN);
}

inline void NavPoint::setEdge(GridDirection d, bool mask)
{
    if (mask)
        mEdgeMask |= 1 << d.n();
    else mEdgeMask &= ~(1 << d.n());
}

inline void NavPoint::copyFrom(const NavPoint &nav)
{
    assert(mRefX == nav.mRefX && mRefY == nav.mRefY && mLayer == nav.mLayer);
    mCost = nav.mCost;
    mFlags = nav.mFlags;
    mKOCount = nav.mKOCount;
}

inline void NavPoint::write(const NavRasterizeParams &data)
{
    if ((mWriteSeq == data.WriteSeq) || (mFlags & data.IgnoreMask))
        return;
    mWriteSeq = data.WriteSeq;
    mFlags &= ~NAV_POINT_FLAGS_CLEARANCE;
    if (mKOCount._RouteTracks += data.KOCount._RouteTracks) mFlags |= NAV_POINT_FLAG_ROUTE_TRACK_CLEARANCE;
    if (mKOCount._RouteVias += data.KOCount._RouteVias)     mFlags |= NAV_POINT_FLAG_ROUTE_VIA_CLEARANCE;
    if (mKOCount._PinTracks += data.KOCount._PinTracks)     mFlags |= NAV_POINT_FLAG_PIN_TRACK_CLEARANCE;
    if (mKOCount._PinVias += data.KOCount._PinVias)         mFlags |= NAV_POINT_FLAG_PIN_VIA_CLEARANCE;
    mFlags = (mFlags & data.FlagsAnd) | data.FlagsOr;
}

#endif // GYM_PCB_NAVPOINT_H
