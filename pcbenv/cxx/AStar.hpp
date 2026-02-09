
#ifndef GYM_PCB_ASTAR_H
#define GYM_PCB_ASTAR_H

#include "Log.hpp"
#include "PCBoard.hpp"
#include "NavGrid.hpp"
#include "Path.hpp"
#include <queue>

// OPTIMIZATIONS TODO:
// "Online Graph Pruning for Pathfinding on Grid Maps", D. Harabor & A. Grastien, AAAI 2011
// 1) Don't evaluate nodes that are more quickly reached from the parent.
// 2) Keep moving in the same direction without expansion as long as we don't
//    find another neighbour (other than straight) that isn't more quickly
//    reached from (dominated by) the parent.
// 3) The point where we find another neighbour is the jump point that we use
//    as successor to our original node.
// 4) If we reach an obstacle going straight, we do nothing.
// The paper proves that this strategy does not affect optimality.

/// Whether we can route diagonally if the adjacent directions (U,L for UL etc.) are blocked.
#define ASTAR_ALLOW_XOVER false

/// As the A-star heuristic is not consistent, it can happen that we find a shorter path
/// for a specific point and have to increase its priority in the open list.
/// As std::priority_queue does not support changing the key, we insert nodes more than once.
/// To avoid violating the heap property we have to score the key separately from NavPoint.
class NavPointRef
{
public:
    NavPointRef(NavPoint *nav, float v) : mPoint(nav), mKey(nav->getScore() + v) { }
    NavPoint *getPoint() const { return mPoint; }
    NavPoint *operator->() const { return mPoint; }
    bool operator<(const NavPointRef &that) const { return mKey >= that.mKey; }
private:
    float mKey;
    NavPoint *mPoint;
};

class AStar
{
public:
    AStar(NavGrid&, const AStarCosts&);
    ~AStar();
    bool search(Connection&);
    const std::vector<Point_25>& violations() const { return mViolationLocs; }
private:
    NavGrid &mNav;
    Point_2 mTargetXY;
    int mTargetZ[2];
    std::vector<uint8_t> mPreferredDirections;
    uint16_t mRouteMask{NAV_POINT_FLAGS_TRACKS_BLOCKED};
    float mViaCost;
    float mWrongDirectionCostDiag;
    float mViolationCost;
    uint32_t mLayerMask;
    NavPoint *mTarget{0};
    Point_2 mSourceXY;
    std::vector<Point_25> mViolationLocs;
    AStarCosts mCostParams;
    float heuristic(const NavPoint&);
    int getApproxBlockageSearchArea(const Pin *) const;
    int _search(NavPoint *source, int maxVisits);
    int _search(NavPoint *source, NavPoint *target, int maxVisits);
    void initEndPoint(const Point_2&, int z[2], bool dst, bool save);
    void finiEndPoint(const Point_2&, int z[2]);
    bool reconstruct(Connection&);
    void addViolation(const NavPoint *, Real radius);
    void initCosts(const Connection&);
    float sumCostsSquare(const NavPoint *, int extent) const;
    float computeCost(NavPoint *src, NavPoint *dst, const GridDirection d);
private:
    void writePoint(const Point_25&, uint16_t add, uint16_t clr, bool save);
    NavPoint *checkHEdge(const NavPoint *, GridDirection) const;
    NavPoint *checkVEdge(const NavPoint *, GridDirection) const;
};

AStar::AStar(NavGrid &nav, const AStarCosts &costs) : mNav(nav), mCostParams(costs)
{
}
AStar::~AStar()
{
}

inline float AStar::sumCostsSquare(const NavPoint *src, int ex) const
{
    float c = 0.0f;
    for (int x = -ex; x <= ex; ++x)
    for (int y = -ex; y <= ex; ++y)
        c += mNav.getPoint(src->x() + x, src->y() + y, src->z()).getCost();
    return c;
}

inline void AStar::writePoint(const Point_25 &v, uint16_t add, uint16_t clr, bool save)
{
    NavPoint *x = mNav.getPoint(v);
    if (save)
        x->saveFlags();
    x->setFlags(add);
    x->clearFlags(clr);
}

/**
 * Mark the source/target pin area with flags and initialize mTargetZ.
 */
inline void AStar::initEndPoint(const Point_2 &v, int Z[2], bool dst, bool save)
{
    const uint w = dst ? NAV_POINT_FLAG_TARGET : NAV_POINT_FLAG_SOURCE;
    const uint m = dst ? NAV_POINT_FLAG_SOURCE : NAV_POINT_FLAG_TARGET;
    // If the endpoint goes across multiple layers it will be a pin where we can move freely vertically, so remove via clearance. Leaving the pin is still blocked by the canPlaceVia() check.
    const uint u = (Z[0] == Z[1]) ? NAV_POINT_FLAGS_TRACK_CLEARANCE : NAV_POINT_FLAGS_CLEARANCE;
    for (int z = Z[0]; z <= Z[1]; ++z)
        writePoint(Point_25(v, z), w, NAV_POINT_FLAG_BLOCKED_TEMPORARY | u | m, save);
    if (dst) {
        mTargetZ[0] = Z[0];
        mTargetZ[1] = Z[1];
    }
}
inline void AStar::finiEndPoint(const Point_2 &v, int Z[2])
{
    for (int z = Z[0]; z <= Z[1]; ++z)
        mNav.getPoint(v, z)->restoreFlags();
}

float AStar::heuristic(const NavPoint &A)
{
    //float d = std::sqrt(CGAL::squared_distance(A.getRefPoint(), mTargetXY));
    float d = geo::distance45(A.getRefPoint(&mNav), mTargetXY);

    uint dz = 0;
    if (A.getLayer() < mTargetZ[0])
        dz = mTargetZ[0] - A.getLayer();
    else if (A.getLayer() > mTargetZ[1])
        dz = A.getLayer() - mTargetZ[1];
    if (dz) {
        if (!A.getBackDirection().isVertical())
            dz += 1;
        d += mViaCost * 0.5f * dz;
    }
    return d;
}

// When we add a via:
// A z=0 -> Segment(s,A)
// B z=0 -> Segment(s,B)
// B z=1 ->     Via(0,1)
// C z=1 -> Segment(B,C)
bool AStar::reconstruct(Connection &route)
{
    std::lock_guard wlock(mNav.getPCB().getLock());

    assert(!route.hasTracks() && !route.isRouted());
    assert(mTarget);
    const NavPoint *head = mTarget;
    for (auto next = head->getBackEdge(mNav); next->hasFlags(NAV_POINT_FLAG_TARGET); next = next->getBackEdge(mNav))
        head = next;
    Track *T = route.newTrack(head->getRefPoint25(&mNav));
    const NavPoint *node = head;
    DEBUG("AStar " << node->str(&mNav));
    GridDirection d = node->getBackDirection();
    while (!node->hasFlags(NAV_POINT_FLAG_SOURCE)) {
        node = node->getBackEdge(mNav);
        if (!node->canRoute())
            addViolation(node, T->defaultWidth());
        DEBUG("AStar " << node->str(&mNav));
        if (node->getBackDirection() == d && node->getLayer() == head->getLayer() && !node->hasFlags(NAV_POINT_FLAG_SOURCE))
            continue;
        if (!d.isVertical())
            T->_append(head->getSegmentTo(*node, &mNav));
        head = node;
        d = node->getBackDirection();
    }
    assert(head == node);
    T->_setEnd(node->getRefPoint25(&mNav));
    route.forceRouted();
    T->autocreateVias(T->end() == node->getRefPoint25(&mNav) ? T->end() : route.target());
    T->computeLength();
    return true;
}

/**
 * @param radius Minimum distance from previous violation.
 */
void AStar::addViolation(const NavPoint *node, Real radius)
{
    const auto v = node->getRefPoint25(&mNav);
    if (mViolationLocs.empty() ||
        mViolationLocs.back().z() != v.z() ||
        CGAL::squared_distance(v.xy(), mViolationLocs.back().xy()) > radius)
        mViolationLocs.push_back(v);
}

/**
 * Return the number of grid cells to scan to check for a blocked pin.
 */
int AStar::getApproxBlockageSearchArea(const Pin *T) const
{
    const auto e = mNav.EdgeLen;
    if (!T)
        return 384;
    const int nx = int((T->getBbox().xmax() - T->getBbox().xmin() + e) / e);
    const int ny = int((T->getBbox().xmax() - T->getBbox().xmin() + e) / e);
    const int nA = nx * ny;
    return std::max(384, std::min(nA * 8, 1024));
}

inline float AStar::computeCost(NavPoint *src, NavPoint *dst, const GridDirection d)
{
    auto moveCost = dst->getCost();

    if (d.isVertical()) {
        // moveCost = sumCostsSquare(dst, 1);
        moveCost *= mViaCost;
        // Using the same via costs less, but we can't make the cost 0 or we'd always explore the whole layer stack rather:
        if (src->getBackDirection().isVertical())
            moveCost *= 0.5f;
    } else {
        const bool nonPref = !(mPreferredDirections[dst->getLayer()] & d.mask());
        if (d._isDiagonal())
            moveCost *= nonPref ? mWrongDirectionCostDiag : std::sqrt(2.0f);
        else if (nonPref)
            moveCost *= mCostParams.WrongDirection;
        if (!(mLayerMask & (1 << dst->getLayer())))
            moveCost *= mCostParams.MaskedLayer;
        if (dst->hasFlags(NAV_POINT_FLAG_ROUTE_TRACK_CLEARANCE))
            moveCost *= mViolationCost;
        moveCost += mCostParams.TurnPer45Degrees * math::squared(d.opposite().get45DegreeStepsBetween(src->getBackDirection()));
    }
    // Moving through the source node is cheaper:
    if (dst->hasFlags(NAV_POINT_FLAG_SOURCE))
        moveCost *= 0.125f;
    return moveCost;
}

inline NavPoint *AStar::checkHEdge(const NavPoint *ref, GridDirection d) const
{
    auto e = ref->getEdge(mNav, d);
    return (e && !e->hasFlags(mRouteMask)) ? e : 0;
}
inline NavPoint *AStar::checkVEdge(const NavPoint *ref, GridDirection d) const
{
    if (!ref->canPlaceVia())
        return 0;
    auto e = ref->getEdge(mNav, d);
    return (e && ref->canAddVia(*e)) ? e : 0;
}

#define PERF_COUNTER_T(i)    auto __t##i = std::chrono::high_resolution_clock::now()
#define PERF_COUNTER_D(a, b) std::chrono::duration_cast<std::chrono::nanoseconds>(__t##b - __t##a).count()

bool AStar::search(Connection &route)
{
    DEBUG("A* from " << route.source() << " to " << route.target());

    if (route.hasTracks() || route.isRouted())
        throw std::runtime_error("Connection pass to A* must have an empty track.");

    // Search in reverse so reconstruct goes in the right direction.
    auto target = mNav.getPoint(route.source());
    auto source = mNav.getPoint(route.target());
    if (!target || !source || source == target)
        return false;
    initCosts(route);

    int sourceZ[2];
    int targetZ[2];
    sourceZ[0] = route.targetPin() ? route.targetPin()->minLayer() : source->getLayer();
    targetZ[0] = route.sourcePin() ? route.sourcePin()->minLayer() : target->getLayer();
    sourceZ[1] = route.targetPin() ? route.targetPin()->maxLayer() : source->getLayer();
    targetZ[1] = route.sourcePin() ? route.sourcePin()->maxLayer() : target->getLayer();

    // First test the other direction to see if we're blocked off close to the endpoint (rv < 0).
    initEndPoint(target->getRefPoint(&mNav), targetZ, 0, true);
    initEndPoint(source->getRefPoint(&mNav), sourceZ, 1, true);
    auto rv = _search(target, source, getApproxBlockageSearchArea(route.sourcePin()));
    if (rv >= 0) {
        initEndPoint(source->getRefPoint(&mNav), sourceZ, 0, false);
        initEndPoint(target->getRefPoint(&mNav), targetZ, 1, false);
        rv = _search(source, target, std::numeric_limits<int>::max());
    }
    DEBUG("A* " << ((rv > 0) ? "finished" : "failed") << " after " << ((rv > 0) ? (std::numeric_limits<int>::max() - rv) : -rv) << " nodes.");

    auto ret = (rv > 0) && reconstruct(route);
    if (rv <= 0) {
        std::lock_guard wlock(mNav.getPCB().getLock());
        route.clearTracks();
    }
    finiEndPoint(source->getRefPoint(&mNav), sourceZ);
    finiEndPoint(target->getRefPoint(&mNav), targetZ);
    return ret;
}
int AStar::_search(NavPoint *source, NavPoint *target, int maxVisits)
{
    assert(source && target && source != target);
    mTargetXY = target->getRefPoint(&mNav);
    mSourceXY = source->getRefPoint(&mNav);
    return _search(source, maxVisits);
}
int AStar::_search(NavPoint *source, int maxVisits)
{
    const uint seq = mNav.nextSearchSeq();

    source->setScore(0);
    source->setBackDirection(GridDirection::Z().n());
    source->getVisits().setOpen(seq);

    std::priority_queue<NavPointRef> openList;
    openList.push(NavPointRef(source, heuristic(*source)));
    while (!openList.empty()) {
        auto current = openList.top().getPoint();
        openList.pop();
        if (!current->getVisits().isOpen(seq)) // duplicate entry already encountered sooner
            continue;
        if (current->hasFlags(NAV_POINT_FLAG_TARGET)) {
            mTarget = current;
            return maxVisits;
        }
        if (!--maxVisits)
            break;
        current->getVisits().setDone(seq);

        // FIXME: The no-cross check is still no sufficient for correctness.
        // We can have conditions where unroute-reroute does not work:
        //   A B
        //  A B
        // A BCCC <-- C can be routed or be a via, but then B becomes illegal
        //    CCC
        //    CCC
        NavPoint *edges[GridDirection::Count];
        edges[GridDirection::U().n()] = checkHEdge(current, GridDirection::U());
        edges[GridDirection::D().n()] = checkHEdge(current, GridDirection::D());
        edges[GridDirection::L().n()] = checkHEdge(current, GridDirection::L());
        edges[GridDirection::R().n()] = checkHEdge(current, GridDirection::R());
        edges[GridDirection::UR().n()] = ((edges[GridDirection::U().n()] && edges[GridDirection::R().n()]) || ASTAR_ALLOW_XOVER) ? checkHEdge(current, GridDirection::UR()) : 0;
        edges[GridDirection::DR().n()] = ((edges[GridDirection::D().n()] && edges[GridDirection::R().n()]) || ASTAR_ALLOW_XOVER) ? checkHEdge(current, GridDirection::DR()) : 0;
        edges[GridDirection::DL().n()] = ((edges[GridDirection::D().n()] && edges[GridDirection::L().n()]) || ASTAR_ALLOW_XOVER) ? checkHEdge(current, GridDirection::DL()) : 0;
        edges[GridDirection::UL().n()] = ((edges[GridDirection::U().n()] && edges[GridDirection::L().n()]) || ASTAR_ALLOW_XOVER) ? checkHEdge(current, GridDirection::UL()) : 0;
        edges[GridDirection::A().n()] = checkVEdge(current, GridDirection::A());
        edges[GridDirection::V().n()] = checkVEdge(current, GridDirection::V());
        const auto backd = current->getBackDirection();
        if (!backd.isZero())
            edges[backd.n()] = 0; // no need to check going right back
        if (backd.is2D()) {
            edges[backd.rotatedCcw45().n()] = 0; // these will never be better as edge costs are node costs and never negative
            edges[backd.rotatedCw45().n()] = 0;
        }

        for (auto d = GridDirection::begin(); d != (current->canPlaceVia() ? GridDirection::vend() : GridDirection::hend()); ++d) {
            NavPoint *node = edges[d.n()];
            if (!node)
                continue;
            const auto score = current->getScore() + computeCost(current, node, d);
            if (node->getVisits().isSeen(seq) && score >= node->getScore())
                continue;
            node->setBackDirection(d.opposite());
            node->setScore(score);
            node->getVisits().setOpen(seq);
            openList.push(NavPointRef(node, heuristic(*node)));
        }
#if GYM_PCB_ENABLE_UI
        mNav.getPCB().setChanged(PCB_CHANGED_NAV_GRID);
        UserSettings::get().sleepForActionDelay();
#endif
    }
    return -maxVisits;
}

void AStar::initCosts(const Connection &X)
{
    assert(mCostParams.valid());

    mLayerMask = X.getLayerMask();

    mPreferredDirections.assign(mNav.getSize(2), 0xff);
    for (uint z = 0; z < std::min(mPreferredDirections.size(), mCostParams.PreferredDirections.size()); ++z) {
        switch (mCostParams.PreferredDirections[z]) {
        case 'x': mPreferredDirections[z] = GridDirection::R().mask() | GridDirection::L().mask(); break;
        case 'y': mPreferredDirections[z] = GridDirection::U().mask() | GridDirection::D().mask(); break;
        case '0': mPreferredDirections[z] = 0; break;
        }
    }
    mWrongDirectionCostDiag = std::sqrt(2.0f) + (mCostParams.WrongDirection - 1.0f);

    mViaCost = mCostParams.Via * X.defaultViaDiameter();

    mViolationCost = mCostParams.Violation;
    if (std::isinf(mCostParams.Violation))
        mRouteMask |= NAV_POINT_FLAG_ROUTE_TRACK_CLEARANCE;
    else
        mRouteMask &= ~NAV_POINT_FLAG_ROUTE_TRACK_CLEARANCE;
}

#endif // GYM_PCB_ASTAR_H
