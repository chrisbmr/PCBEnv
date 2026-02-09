
#include "PyArray.hpp"
#include "NavGrid.hpp"
#include "PCBoard.hpp"
#include "Component.hpp"
#include "Net.hpp"
#include "Connection.hpp"
#include "Track.hpp"
#include "UserSettings.hpp"

#include "AStar.hpp"
#include "RasterizerMidpoint.hpp"

NavSpacings::NavSpacings(const Connection &X)
{
    Clearance = X.clearance();
    TrackWidthHalf = 0.5 * X.defaultTraceWidth();
    ViaRadius = X.defaultViaRadius();
}

NavGrid::NavGrid(PCBoard &pcb) : UniformGrid25(1.0), mPCB(pcb)
{
    mSpacings.Clearance = 0.0;
    mSpacings.TrackWidthHalf = 0.0;
    mSpacings.ViaRadius = 0.0;

    mAStarCosts.reset();
}

void NavGrid::copyFrom(const NavGrid &nav)
{
    assert(mPoints.size() == nav.mPoints.size());
    mSpacings = nav.getSpacings();
    for (uint i = 0; i < mPoints.size(); ++i)
        mPoints[i].copyFrom(nav.mPoints[i]);
}

void NavGrid::build()
{
    mSize[2] = mPCB.getNumLayers();;
    mSize[1] = std::ceil(mPCB.getLayoutArea().size().y() / EdgeLen);
    mSize[0] = std::ceil(mPCB.getLayoutArea().size().x() / EdgeLen);
    calcNumPoints3D();
    mPoints.resize(getNumPoints3D());

    DEBUG("Building NavGrid of size " << mSize[0] << 'x' << mSize[1] << 'x' << mSize[2]);

    const auto O = mPCB.getLayoutArea().origin();

    mBbox = Bbox_2(O.x(), O.y(), O.x() + mSize[0] * EdgeLen, O.y() + mSize[1] * EdgeLen);

    mStrideY = mSize[0];
    mStrideZ = mSize[1] * mStrideY;
    initDirectionStrides();

    // Rasterize these first so we can remove some edges.
    rasterizeFootprints();

    // Loop twice so we can do some assertions the second time.
    for (uint z = 0, i = 0; z < mSize[2]; ++z)
    for (uint y = 0; y < mSize[1]; ++y)
    for (uint x = 0; x < mSize[0]; ++x, ++i)
        mPoints[i].setRefPoint(x, y, z);
    for (uint z = 0, i = 0; z < mSize[2]; ++z)
    for (uint y = 0; y < mSize[1]; ++y)
    for (uint x = 0; x < mSize[0]; ++x, ++i)
        initEdges(mPoints[i], IPoint_3(x,y,z));

    // Mark existing tracks as rasterized so we don't skip them in rasterizeClearanceAreas().
    for (auto net : mPCB.getNets())
        for (const auto X : net->connections())
            for (auto T : X->getTracks())
                T->addRasterizedCount(1);

    DEBUG("NavGrid built.");
}
void NavGrid::initSpacingsForAnyRoutedTrack()
{
    for (auto net : mPCB.getNets()) {
        for (auto X : net->connections()) {
            if (X->isRouted()) {
                setSpacings(NavSpacings(*X));
                return;
            }
        }
    }
}
inline void NavGrid::initEdges(NavPoint &P, const IPoint_3 &v)
{
    if (P.hasFlags(NAV_POINT_FLAG_BLOCKED_PERMANENT))
        return;
    for (auto d = GridDirection::begin(); d != (P.canPlaceViaEver() ? GridDirection::vend() : GridDirection::hend()); ++d) {
        const auto vn = v + d.ivec3();
        if (!inside(vn))
            continue;
        const NavPoint &N = getPoint(vn);
        assert((P.getLayer() != N.getLayer()) == d.isVertical());
        if (d.isVertical() && !P.canAddViaEver(N))
            continue;
        if (N.hasFlags(NAV_POINT_FLAG_BLOCKED_PERMANENT))
            continue;
        P.setEdge(d, true);
        assert(P.getEdge(*this, d) == &N);
    }
}
void NavGrid::initDirectionStrides()
{
    mDirectionStride[GridDirection::L().n()]  = -1 * int(sizeof(NavPoint));
    mDirectionStride[GridDirection::R().n()]  = +1 * int(sizeof(NavPoint));
    mDirectionStride[GridDirection::D().n()]  = -mSize[0] * int(sizeof(NavPoint));
    mDirectionStride[GridDirection::U().n()]  = +mSize[0] * int(sizeof(NavPoint));
    mDirectionStride[GridDirection::DL().n()] = (-mSize[0] - 1) * int(sizeof(NavPoint));
    mDirectionStride[GridDirection::UL().n()] = (+mSize[0] - 1) * int(sizeof(NavPoint));
    mDirectionStride[GridDirection::DR().n()] = (-mSize[0] + 1) * int(sizeof(NavPoint));
    mDirectionStride[GridDirection::UR().n()] = (+mSize[0] + 1) * int(sizeof(NavPoint));
    mDirectionStride[GridDirection::V().n()]  = -mStrideZ * int(sizeof(NavPoint));
    mDirectionStride[GridDirection::A().n()]  = +mStrideZ * int(sizeof(NavPoint));
}

bool NavGrid::setSpacings(const NavSpacings &_spacings)
{
    NavSpacings spacings = _spacings;
    if (mSize[2] == 1)
        spacings.ViaRadius = 0.0; // no vias for single layer boards
    if (mSpacings == spacings)
        return false;
    DEBUG("Grid spacings have changed:\nclearance: " << mSpacings.Clearance << " -> " << spacings.Clearance << "\ntrack halfwidth: " << mSpacings.TrackWidthHalf << " -> " << spacings.TrackWidthHalf << "\nvia radius: " << mSpacings.ViaRadius << " -> " << spacings.ViaRadius);
    mSpacings = spacings;
    rasterizeClearanceAreas();
    return true;
}

void NavGrid::resetUserKeepout(uint idx)
{
    for (NavPoint &P : mPoints)
        P.getKOCounts()._User[idx] = 0;
}
void NavGrid::resetUserKeepouts()
{
    for (NavPoint &P : mPoints)
        P.getKOCounts()._User[0] = P.getKOCounts()._User[1] = 0;
}
void NavGrid::resetKeepouts()
{
    for (auto net : mPCB.getNets())
        for (auto X : net->connections())
            for (auto T : X->getTracks())
                T->resetRasterizedCount();

    for (NavPoint &P : mPoints)
        P.resetKO();
}

void NavGrid::rasterizeFootprints()
{
    NavRasterizeParams rast;

    for (auto C : mPCB.getComponents()) {
        rast.FlagsOr = NAV_POINT_FLAG_INSIDE_COMPONENT;
        if (!C->canRouteInside())
            rast.FlagsOr |= NAV_POINT_FLAG_BLOCKED_TEMPORARY;
        if (!C->canPlaceViasInside())
            rast.FlagsOr |= NAV_POINT_FLAG_NO_VIAS;
        rasterize(*C, rast);
    }
    for (auto C : mPCB.getComponents()) {
        for (auto P : C->getPins()) {
            rast.FlagsOr = NAV_POINT_FLAG_INSIDE_PIN;
            rast.FlagsAnd = ~(NAV_POINT_FLAG_BLOCKED_TEMPORARY | NAV_POINT_FLAG_NO_VIAS);
            rasterize(*P->as<Pin>(), rast);
        }
    }
}

void NavGrid::rasterizeClearanceAreas()
{
    for (NavPoint &P : mPoints)
        P.resetKO();

    NavRasterizeParams rast;
    rast.AutoExpand = true;

    rast.KOCount.setPin(1);
    for (auto I : mPCB.getComponents()) {
        for (auto P : I->getPins()) {
            if (P->canRouteInside())
                continue;
            rasterize(*P->as<Pin>(), rast);
        }
    }

    rast.KOCount.setRoute(1);
    for (auto net : mPCB.getNets())
        for (const auto X : net->connections())
            if (X->isRasterized_allOrNone()) // RRR: only re-rasterize tracks are marked as rasterized
                rasterize(*X, rast);

    rasterizeLayoutAreaBorder(rast);
}

class NavROP final : public BaseROP
{
public:
    void writeIndex(uint i) const;
    void writeRangeZYX(uint Z0, uint Z1, uint Y0, uint Y1, uint X0, uint X1) override;
    void setTarget(NavGrid &nav) { mGrid = &nav; }
    void setParams(const NavRasterizeParams &params) { mParams = &params; }
private:
    NavGrid *mGrid;
    const NavRasterizeParams *mParams;
};
inline void NavROP::writeIndex(uint i) const
{
    mGrid->getPoint(i).write(*mParams);
}
inline void NavROP::writeRangeZYX(uint Z0, uint Z1, uint Y0, uint Y1, uint X0, uint X1)
{
    assert(mGrid);
    for (uint Z = Z0; Z <= Z1; ++Z) {
    for (uint Y = Y0; Y <= Y1; ++Y) {
        const auto I0 = mGrid->LinearIndex(Z, Y, X0);
        const auto I1 = I0 + (X1 - X0);
        for (uint i = I0; i <= I1; ++i)
            writeIndex(i);
    }}
}

/**
 * We need to rasterize the border to make sure tracks don't exceed the layout area.
 * FIXME: This is inaccurate for wider tracks at angles other than 0°/90° with the border.
 */
void NavGrid::rasterizeLayoutAreaBorder(const NavRasterizeParams &_params)
{
    NavRasterizeParams params = _params;
    Rasterizer<NavROP> R(*this);
    R.OP.setTarget(*this);
    R.OP.setParams(params);

    // NOTE: rasterizeLine doesn't care about the midpoint so we have to deduct 0.5
    const auto ex = params.Expansion + mSpacings.TrackWidthHalf - 0.5 * EdgeLen;
    if (ex < 0.0)
        return;
    if (params.AutoIncrementWriteSeq)
        params.WriteSeq = nextRasterSeq();
    params.KOCount.setPin(1, (mSpacings.ViaRadius > mSpacings.TrackWidthHalf) ? 0 : 1);
    R.setExpansion(ex);
    R.rasterizeLine(mPCB.getLayoutArea().bbox(), 0, mSize[2] - 1);
    if (!params.KOCount._PinVias) {
        params.KOCount.setPinVias(1);
        R.setExpansion(params.Expansion + mSpacings.ViaRadius);
        R.rasterizeLine(mPCB.getLayoutArea().bbox(), 0, mSize[2] - 1);
    }
}

void NavGrid::rasterize(const AShape *S, uint Z0, uint Z1, const NavRasterizeParams &params)
{
    if (params.AutoIncrementWriteSeq)
        params.WriteSeq = nextRasterSeq();
    Rasterizer<NavROP> R(*this);
    R.OP.setTarget(*this);
    R.OP.setParams(params);
    R.setExpansion(params.Expansion);
    R.rasterizeFill(S, Z0, Z1);
}

void NavGrid::rasterize(const Component &C, const NavRasterizeParams &params)
{
    assert(!params.AutoExpand);
    rasterize(C.getShape(), C.getSingleLayer(), C.getSingleLayer(), params);
}

void NavGrid::rasterizeCompound(const Pin &T, const NavRasterizeParams &params)
{
    if (T.compound()) {
        for (const auto *P : *T.compound())
            rasterize(*P, params);
    } else {
        rasterize(T, params);
    }
}
void NavGrid::rasterize(const Pin &T, const NavRasterizeParams &_params)
{
    assert(_params.KOCount._PinTracks || !_params.AutoExpand);
    const bool Pass2 = _params.AutoExpand && (mSpacings.ViaRadius > mSpacings.TrackWidthHalf);
    NavRasterizeParams params = _params;
    if (params.AutoExpand)
        params.Expansion = mSpacings.getExpansionForTracks(T.getClearance());
    if (Pass2)
        params.KOCount.setPinTracks(_params.KOCount._PinTracks);
    rasterize(T.getShape(), T.minLayer(), T.maxLayer(), params);
    if (Pass2) {
        // If we are erasing the clearance flags to make pins accessible to their nets,
        // only do it for the PIN_VIA ones here, not PIN_TRACK.
        params.FlagsOr  &= ~NAV_POINT_FLAG_PIN_TRACK_CLEARANCE;
        params.FlagsAnd |=  NAV_POINT_FLAG_PIN_TRACK_CLEARANCE;
        params.KOCount.setPinVias(_params.KOCount._PinVias);
        params.Expansion = mSpacings.getExpansionForVias(T.getClearance());
        if (!params.isNoop())
            rasterize(T.getShape(), T.minLayer(), T.maxLayer(), params);
    }
}

/// Rasterize the connections track and vias, with segment junctions.
void NavGrid::rasterize(const Connection &X, const NavRasterizeParams &_params)
{
    assert(_params.KOCount._RouteTracks || !_params.AutoExpand);
    if (!X.hasTracks())
        throw std::runtime_error("Tried to rasterize empty path");

    const bool TwoPass = _params.AutoExpand && (mSpacings.ViaRadius > mSpacings.TrackWidthHalf);
    NavRasterizeParams params1 = _params;
    NavRasterizeParams params2 = _params;
    if (TwoPass) {
        params1.KOCount.setRouteTracks(_params.KOCount._RouteTracks);
        params2.KOCount.setRouteVias(_params.KOCount._RouteVias);
    }
    if (_params.AutoExpand) {
        params1.Expansion = mSpacings.getExpansionForTracks(X.clearance());
        params2.Expansion = mSpacings.getExpansionForVias(X.clearance());
        assert(params1.Expansion >= 0.0);
    }
    params1.AutoIncrementWriteSeq = false;
    params2.AutoIncrementWriteSeq = false;

    params1.WriteSeq = params2.WriteSeq = nextRasterSeq();
    for (const auto *T : X.getTracks()) {
        rasterize(*T, params1, RASTERIZE_MASK_VIAS);
        if (TwoPass)
            rasterize(*T, params2, RASTERIZE_MASK_VIAS);
    }

    // NOTE: We use a new write seq so we can erase vias without erasing overlapping segments.
    params1.WriteSeq = params2.WriteSeq = nextRasterSeq();
    for (const auto *T : X.getTracks()) {
        rasterize(*T, params1, RASTERIZE_MASK_SEGMENTS | RASTERIZE_MASK_CAPS_AND_JUNCTIONS);
        if (TwoPass)
            rasterize(*T, params2, RASTERIZE_MASK_SEGMENTS | RASTERIZE_MASK_CAPS_AND_JUNCTIONS);
        T->addRasterizedCount(_params.TrackRasterCount);
    }
}

void NavGrid::rasterize(const Track &T, const NavRasterizeParams &params, uint mask)
{
    if (params.AutoIncrementWriteSeq)
        params.WriteSeq = nextRasterSeq();
    Rasterizer<NavROP> R(*this);
    R.OP.setTarget(*this);
    R.OP.setParams(params);
    R.setExpansion(params.Expansion);
    R.rasterizeFill(T, mask);
}

void NavGrid::rasterize(const Path &path, const NavRasterizeParams &params)
{
    if (params.AutoIncrementWriteSeq)
        params.WriteSeq = nextRasterSeq();
    Rasterizer<NavROP> R(*this);
    R.OP.setTarget(*this);
    R.OP.setParams(params);
    R.setExpansion(params.Expansion);
    for (uint i = 1; i < path.numPoints(); ++i)
        if (path.isPlanarAt(i))
            R.rasterizeLine(path.getSegmentTo(i));
}

void NavGrid::resetSearchSeq()
{
    mSearchSeq = 0;
    for (auto &v : mPoints)
        v.getVisits().reset();
}
void NavGrid::resetRasterSeq()
{
    mRasterSeq = 0;
    for (auto &v : mPoints)
        v.setWriteSeq(0);
}

bool NavGrid::findPathAStar(Connection &X, const AStarCosts *costs)
{
    assert(X.defaultViaDiameter() > 0.0);
    return AStar(*this, costs ? *costs : mAStarCosts).search(X);
}

void NavGrid::getCosts(std::vector<float> &costs)
{
    costs.resize(mPoints.size());
    for (uint i = 0; i < mPoints.size(); ++i)
        costs[i] = mPoints[i].getCost();
}
void NavGrid::setCosts(float v)
{
    for (uint i = 0; i < mPoints.size(); ++i)
        mPoints[i].setCost(v);
}
void NavGrid::setCosts(const float *data, float v)
{
    assert(data);
    for (uint i = 0; i < mPoints.size(); ++i)
        mPoints[i].setCost(v + data[i]);
}
void NavGrid::setCosts(const IBox_3 &box, const float *data, const float v)
{
    assert(data);
    if (!box.valid())
        throw std::invalid_argument("bounding box must have min <= max");
    if (!inside(box.min) ||
        !inside(box.max))
        throw std::invalid_argument("bounding box exceeds grid");
    uint i = 0;
    for (int z = box.min.z; z <= box.max.z; ++z)
    for (int y = box.min.y; y <= box.max.y; ++y)
    for (int x = box.min.x; x <= box.max.x; ++x, ++i)
        getPoint(x,y,z).setCost(v + data[i]);
}
void NavGrid::setCosts(const IBox_3 &box, float v)
{
    if (!box.valid())
        throw std::invalid_argument("bounding box must have min <= max");
    if (!inside(box.min) ||
        !inside(box.max))
        throw std::invalid_argument("bounding box exceeds grid");
    for (int z = box.min.z; z <= box.max.z; ++z)
    for (int y = box.min.y; y <= box.max.y; ++y)
    for (int x = box.min.x; x <= box.max.x; ++x)
        getPoint(x,y,z).setCost(v);
}


class NavCounterROP final : public BaseROP
{
public:
    void setTarget(NavGrid &nav) { mGrid = &nav; }
    void setCheckMask(uint16_t m) { mFlags = m; }
    void resetCount() { mCount = 0; }
    uint32_t getCount() const { return mCount; }
    void writeRangeZYX(uint Z0, uint Z1, uint Y0, uint Y1, uint X0, uint X1) override;
private:
    NavGrid *mGrid;
    uint16_t mFlags{0};
    uint32_t mCount{0};
};
inline void NavCounterROP::writeRangeZYX(uint Z0, uint Z1, uint Y0, uint Y1, uint X0, uint X1)
{
    for (uint Z = Z0; Z <= Z1; ++Z) {
    for (uint Y = Y0; Y <= Y1; ++Y) {
        const auto I0 = mGrid->LinearIndex(Z, Y, X0);
        const auto I1 = I0 + (X1 - X0);
        for (uint i = I0; i <= I1; ++i)
            if (mGrid->getPoint(i).hasFlags(mFlags))
                mCount += 1;
    }}
}

// cccc|---*---|ccccCC            w/2=4.5 clearance=4
//            CCCCCC|--*--|CCCCCC w/2=3.5 Clearance=6
//                  !!
// This should result in violation area = 2 so we have to rasterize the track without clearance.
Real NavGrid::sumViolationArea(const Connection &X)
{
    setSpacings(NavSpacings(DesignRules(0,0,0)));

    Rasterizer<NavCounterROP> R(*this);
    R.OP.setTarget(*this);

    R.OP.setCheckMask(NAV_POINT_FLAGS_VIA_CLEARANCE);
    for (const auto *T : X.getTracks())
        for (const auto &V : T->getVias())
            R.rasterizeFill(V.getCircle(), V.zmin(), V.zmax());

    R.OP.setCheckMask(NAV_POINT_FLAGS_TRACK_CLEARANCE);
    for (const auto *T : X.getTracks()) {
        const bool addJunctions = true;
        int z = -1;
        for (const auto &s : T->getSegments()) {
            R.rasterizeFill(s, (addJunctions && s.z() != z) ? 0x1 : 0x0);
            z = s.z();
        }
    }

    return R.OP.getCount();
}

std::string NavGrid::str(const IBox_3 *box) const
{
    const auto x0 = box ? box->min.x : 0;
    const auto x1 = box ? (box->max.x + 1) : getSize(0);
    const auto y0 = box ? box->min.y : 0;
    const auto y1 = box ? (box->max.y + 1) : getSize(1);
    std::stringstream ss;
    ss << "  ";
    for (uint x = x0; x < x1; ++x)
        ss << '|' << (x >= 1000 ? "" : " ") << std::setw(2) << x << (x >= 100 ? "" : " ");
    ss << "\n  ";
    for (uint x = x0; x < x1; ++x)
        ss << "-----";
    ss << '\n';
    for (int y = y1 - 1; y >= y0; --y) {
        ss << std::dec << std::setw(2) << y;
        for (uint x = x0; x < x1; ++x) {
            const uint n = getPoint(x,y,0).getKOCounts()._RouteTracks;
            ss << '|' << std::hex << std::setw(2) << uint(getPoint(x,y,0).getFlags()) << ' ';
            if (n)
                ss << n;
            else ss << ' ';
        }
        ss << '\n';
    }
    return ss.str();
}

PyObject *NavGrid::getPy(const IBox_3 &box) const
{
    const uint N = box.volume();
    const uint W = box.w();
    const uint H = box.h();
    const uint D = box.d();
    uint16_t *data = (uint16_t *)std::malloc(N * sizeof(uint16_t));
    for (int Z = 0, z = box.min.z; z <= box.max.z; ++Z, ++z) {
    for (int Y = 0, y = box.min.y; y <= box.max.y; ++Y, ++y) {
    for (int X = 0, x = box.min.x; x <= box.max.x; ++X, ++x) {
        auto k = (W * H) * Z + W * Y + X;
        auto i = LinearIndex(z, y, x);
        data[k] = mPoints[i].getFlags();
    }}}
    return py::NPArray<uint16_t>(data, D, H, W).ownData().release();
}

/**
 * Flags:
 * x BLOCKED_TEMPORARY
 * = NO_VIAS
 * P INSIDE_PIN
 * C INSIDE_COMPONENT
 * o PIN_TRACK_CLEARANCE
 * O PIN_VIA_CLEARANCE
 * | ROUTE_TRACK_CLEARANCE
 * : ROUTE_VIA_CLEARANCE
 * X BLOCKED_PERMANENT
 * s SOURCE
 * t TARGET
 */
std::string NavPoint::str(const NavGrid *grid) const
{
    std::stringstream ss;

    ss << '[';
    if (grid)
        ss << grid->XIndex(getRefPoint(grid).x()) << ',' << grid->YIndex(getRefPoint(grid).y());
    else
        ss << getRefPoint(grid).x() << ',' << getRefPoint(grid).y();
    ss << ',' << (uint)getLayer() << ']';

    ss << " <- " << getBackDirection().str();
    ss << " H(" << getScore() << ')';

    ss << " F(";
    for (uint i = 0; i <= 10; ++i)
        if (hasFlags(1 << i))
            ss << "x=PCoO|:Xst"[i];
    ss << ')';

    ss << " #{" << (int)mKOCount._RouteTracks << ',' << (int)mKOCount._RouteVias << ',' << (int)mKOCount._PinTracks << ',' << (int)mKOCount._PinVias << '}';

    ss << " $(" << getCost() << ')';

    return ss.str();
}

PyObject *NavGrid::getPathCoordinatesNumpy(const Track &T) const
{
    Rasterizer<RecordRangesROP> R(*this);
    for (const auto &s : T.getSegments())
        R.rasterizeLine(s, 0x0);
    std::vector<uint32_t> v;
    for (auto H : R.OP.getRanges()) {
        assert(H.Z0 == H.Z1);
        for (auto Y = H.Y0; Y <= H.Y1; ++Y) {
        for (auto X = H.X0; X <= H.X1; ++X) {
            v.push_back(X);
            v.push_back(Y);
            v.push_back(H.Z0);
        }}
    }
    return py::NPArray<uint32_t>(v, v.size() / 3, 3).ownData().release();
}

void AStarCosts::reset()
{
    PreferredDirections.clear();
    MaskedLayer = 4.0f;
    TurnPer45Degrees = 1.0f / 1024.0f;
    WrongDirection = 1.0f;
    Via = UserSettings::get().AStarViaCostFactor;
    setViolationCostInf();
}
void AStarCosts::setPy(PyObject *py)
{
    reset();
    if (!py)
        return;
    py::Object args(py);
    if (!args.isDict())
        throw std::invalid_argument("A-star costs must be a dict");
    if (auto via = args.item("via"))
        Via = via.toDouble();
    if (auto drv = args.item("drv"))
        Violation = drv.toDouble();
    if (auto wd = args.item("wd"))
        WrongDirection = wd.toDouble();
    if (auto wl = args.item("wl"))
        MaskedLayer = wl.toDouble();
    if (auto tc = args.item("45"))
        TurnPer45Degrees = tc.toDouble();
    if (auto dir = args.item("dir"))
        PreferredDirections = dir.asString();
    if (WrongDirection < 1.0f)
        throw std::invalid_argument("wrong direction cost multiplier must be >= 1");
    if (Violation < 1.0f)
        throw std::invalid_argument("drc violation cost multiplier must be >= 1");
}
