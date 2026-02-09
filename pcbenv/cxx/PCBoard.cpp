
#include "PCBoard.hpp"
#include "Log.hpp"
#include "Component.hpp"
#include "Net.hpp"
#include "Track.hpp"
#include "NavGrid.hpp"
#include "NavTriangulation.hpp"
#include "Clone.hpp"
#include <stack>
#include <fstream>

PCBoard::PCBoard(Nanometers UnitLength) : mUnitLength_nm(UnitLength), mNavGrid(*this)
{
    mComponentAreaBbox = Bbox_2(0,0,0,0);
    mComBVH = ObjectsBVH::create();
    mPinBVH = ObjectsBVH::create();
}

PCBoard::~PCBoard()
{
    TRACE("~PCBoard " << this);
    if (mComBVH)
        delete mComBVH;
    if (mPinBVH)
        delete mPinBVH;
    if (mTNG)
        delete mTNG;
    for (auto N : getNets())
        delete N;
    for (auto C : getComponents())
        delete C;
}

void PCBoard::rebuildBVH()
{
    if (mComBVH)
        delete mComBVH;
    if (mPinBVH)
        delete mPinBVH;
    mComBVH = ObjectsBVH::create();
    mPinBVH = ObjectsBVH::create();
    for (auto C : mParts) {
        mComBVH->insert(*C);
        for (auto P : C->getPins())
            mPinBVH->insert(*P);
    }
}

void PCBoard::pruneLayers(std::set<uint> &Z)
{
    if (Z.empty())
        throw std::invalid_argument("you cannot remove all layers");
    if (*Z.rbegin() >= mLayers.size())
        throw std::invalid_argument(fmt::format("list of layers to retain contains non-existent layer {}", *Z.rbegin()));
    if (Z.size() == mLayers.size()) {
        WARN("the set of layers you asked to remove is empty");
        return;
    }
    // Go backwards so the indices in @Z don't have to change.
    uint zPrev = getNumLayers();
    for (auto I = Z.rbegin(); I != Z.rend(); ++I) {
        uint z = *I;
        if (z != zPrev - 1)
            removedLayersUpdate(z + 1, zPrev - 1);
        zPrev = z;
    }
    if (zPrev != 0)
        removedLayersUpdate(0, zPrev - 1);
}
void PCBoard::setNumLayers(uint N)
{
    if (N > 32)
        throw std::runtime_error("FIXME: bit masks with more than 32 layers");
    if (getNavGrid().getNumPoints() != 0)
        throw std::runtime_error("must not change number of layers after creating NavGrid");
    mLayoutArea.setMaxLayer(N - 1);
    if (N < mLayers.size()) {
        removedLayersUpdate(N, mLayers.size() - 1);
    } else if (N > mLayers.size()) {
        const uint n = mLayers.size();
        mLayers.resize(N);
        for (uint i = n; i < mLayers.size(); ++i)
            mLayers[i].setNumber(i);
    }
}
void PCBoard::removedLayersUpdate(uint zmin, uint zmax)
{
    INFO("removing layers [" << zmin << " .. " << zmax << ']');

    assert(zmin <= zmax);
    const uint dn = (zmax - zmin) + 1;
    for (uint z = zmax + 1; z < mLayers.size(); ++z) {
        const uint i = z - dn;
        mLayers[i] = mLayers[z];
        mLayers[i].setNumber(i);
    }
    mLayers.resize(mLayers.size() - dn);

    std::vector<Component *> coms2Remove;
    std::set<Net *> netsAffected;
    for (auto C : mParts) {
        const uint z = C->getSingleLayer();
        if (zmin <= z && z <= zmax) {
            coms2Remove.push_back(C);
        } else {
            C->setLayer((z < zmin) ? z : (z - dn));
            for (auto I : C->getPins()) {
                auto P = I->as<Pin>();
                assert(P);
                uint mn = P->minLayer();
                uint mx = P->maxLayer();
                if (mx < zmin)
                    continue;
                const uint nz = cutLayersFromRange(mn, mx, zmin, zmax);
                if (!nz)
                    throw std::runtime_error("detected pin that does not share a layer with its component");
                P->setLayerRange(mn, mx);
                if (P->hasNet())
                    netsAffected.insert(P->net());
            }
        }
    }
    for (auto net : netsAffected)
        for (auto X : net->connections())
            X->updateForRemovedLayers(zmin, zmax);
    if (coms2Remove.empty())
        return;
    for (auto C : coms2Remove)
        delete remove(*C);
    rebuildBVH();
}

void PCBoard::add(Component &C)
{
    assert(mPartsByName.find(C.name()) == mPartsByName.end());
    mParts.push_back(&C);
    mPartsByName[C.name()] = &C;
    C.setId(mParts.size() - 1);

    if (mParts.size() == 1)
        mComponentAreaBbox = C.getBbox();
    else
        mComponentAreaBbox += C.getBbox();

    if (mTech.pins.minClearance > 0.0)
        for (auto P : C.getPins())
            if (P->getClearance() < mTech.pins.minClearance)
                P->setClearance(mTech.pins.minClearance);

    std::set<Object *> containers;
    mComBVH->select(containers, C.getBbox(), C.minLayer(), C.maxLayer());
    for (auto D : containers) {
        if (C.isContainerOf(*D))
            C.addOccludedObject(*D);
        else if (D->isContainerOf(C))
            D->addOccludedObject(C);
        else {
            // Typically one will be smaller and we add it to the larger one's occluded set.
            const auto A1 = C.getShape()->unsignedArea();
            const auto A2 = D->getShape()->unsignedArea();
            if (A1 >= A2)
                C.addOccludedObject(*D);
            else
                D->addOccludedObject(C);
            if ((A1 >= A2) ? (A1 < 4.0 * A2) : (A2 < 4.0 * A1))
                WARN("Components of similar sizes intersect: " << C.name() << " & " << D->name());
        }
    }
    mComBVH->insert(C);
    for (auto P : C.getPins())
        mPinBVH->insert(*P);
}
void PCBoard::recomputeComponentAreaBbox()
{
    if (getNumComponents()) {
        mComponentAreaBbox = getComponent(0u)->getBbox();
        for (auto C : getComponents())
            mComponentAreaBbox += C->getBbox();
    } else {
        mComponentAreaBbox = Bbox_2(0,0,0,0);
    }
}

void PCBoard::setMinViaDiameter(Micrometers um)
{
    const Real d = mTech.nets.ViaDiameter = metersToUnits(um);
    for (auto net : mNets)
        if (net->getViaDiameter() < d)
            net->setViaDiameter(d);
}

void PCBoard::setMinTraceWidth(Micrometers um)
{
    const Real w = mTech.nets.TraceWidth = metersToUnits(um);
    INFO("PCBoard::setMinTrackWidth = " << um << " µm = " << (w * 64.0) << " cells.");
    for (auto net : mNets)
        if (net->getMinTraceWidth() < w)
            net->setMinTraceWidth(w);
}
void PCBoard::setMinClearanceNets(Micrometers um)
{
    const Real d = mTech.nets.Clearance = metersToUnits(um);
    for (auto net : mNets)
        if (net->getMinClearance() < d)
            net->setMinClearance(d);
}
void PCBoard::setMinClearancePins(Micrometers um)
{
    const Real d = mTech.pins.minClearance = metersToUnits(um);
    for (auto C : getComponents())
        for (auto P : C->getPins())
            if (P->getClearance() < d)
                P->setClearance(d);
}

void PCBoard::forceConnectionsToGrid()
{
    for (auto net : mNets)
        for (auto X : net->connections())
            if (!X->hasTracks())
                X->forceEndpointsToGrid(mNavGrid);
}

void PCBoard::add(Net &net)
{
    if (mNetsByName.find(net.name()) != mNetsByName.end())
        throw std::runtime_error("Net has already been added");
    mNets.push_back(&net);
    mNetsByName[net.name()] = &net;
    net.setBoard(this);
    net.setId(mNets.size() - 1);
    net.setColorAuto();
    net.setLayerMaskFromSignalType();
    net.setRulesMin(mTech.nets);
}

Component *PCBoard::remove(Component &C)
{
    DEBUG("Remove component " << C.name());
    std::set<Net *> netsToSearch;
    for (auto o : C.getPins())
        if (auto P = o->as<Pin>())
            if (P->hasNet())
                netsToSearch.insert(P->net());
    std::vector<Net *> netsToRemove;
    for (auto net : netsToSearch) {
        DEBUG("Extract " << C.name() << " from net " << net->name());
        if (net->extract(C))
            netsToRemove.push_back(net);
    }
    for (auto net : netsToRemove)
        delete remove(*net);
    mParts[C.id()] = mParts.back();
    mParts.back()->setId(C.id());
    mParts.pop_back();
    mPartsByName.erase(C.name());
    return &C;
}
Net *PCBoard::remove(Net &net)
{
    DEBUG("Remove net " << net.name());
    mNets[net.id()] = mNets.back();
    mNets.back()->setId(net.id());
    mNets.pop_back();
    mNetsByName.erase(net.name());
    return &net;
}

Net *PCBoard::getNet(const std::string &name) const
{
    auto I = mNetsByName.find(name);
    if (I == mNetsByName.end())
        return 0;
    return I->second;
}
Net *PCBoard::getNet(uint index) const
{
    return mNets.at(index);
}

Component *PCBoard::getComponent(const std::string &name) const
{
    auto I = mPartsByName.find(name);
    if (I == mPartsByName.end())
        return 0;
    return I->second;
}
Component *PCBoard::getComponent(uint index) const
{
    return mParts.at(index);
}

Component *PCBoard::getComponentAt(const Point_2 &v, int z) const
{
    assert(z <= 0 || uint(z) == getNumLayers() - 1);
    if (mComBVH) {
        auto C = dynamic_cast<Component *>(mComBVH->select(v, std::max(0, z)));
        if (!C && z < 0)
            C = dynamic_cast<Component *>(mComBVH->select(v, getNumLayers() - 1));
        return C;
    }
    for (auto I : mParts)
        if ((z < 0) ? I->contains2D(v) : I->contains3D(Point_25(v, z)))
            return I;
    return 0;
}

Pin *PCBoard::getPinAt(const Point_2 &v, int z) const
{
    if (z >= int(getNumLayers()))
        throw std::invalid_argument(fmt::format("getPinAt: invalid layer {}", z));
    const uint zmin = std::max(0, z);
    const uint zmax = (z < 0) ? (getNumLayers() - 1) : z;
    for (uint z = zmin; z <= zmax; ++z) {
        if (mPinBVH) {
            auto P = dynamic_cast<Pin *>(mPinBVH->select(v, z));
            if (P)
                return P;
        } else {
            auto C = getComponentAt(v, z);
            if (!C)
                continue;
            auto P = dynamic_cast<Pin *>(C->getChildAt(v));
            if (P)
                return P;
        }
    }
    return 0;
}

Pin *PCBoard::getPin(const std::string &name) const
{
    auto pos = name.find_first_of("-");
    auto componentName = name.substr(0, pos);
    auto pinName = name.substr(pos + 1);
    const auto C = getComponent(componentName);
    if (!C)
        throw std::runtime_error(fmt::format("component {} not found", componentName));
    return C->getPin(pinName);
}

// Prepare for running A-star between the pins @T0 and @T1.
// The pins must be part of the same net.
// 1) Mark the area covered by the parents of the pins as unblocked, except for cells in pins and clearance areas.
// 2) Mark tracks as routable everywhere within the two pins.
// 3) Mark tracks as routable in the pins' clearance area only where no other tracks exist.
// NOTE: Moving along through-hole pins is enabled by A-star itself which clears only the two endpoint cells.
// TODO: Should we make all pins of the net routable?
void PCBoard::initPathfindingForNet(const Connection &X)
{
    const Pin *T0 = X.sourcePin();
    const Pin *T1 = X.targetPin();
    assert(T0 != T1);
    if (T1 && !T0)
        std::swap(T0, T1);
    if (!T0) // T0 must be non-zero, T1 may be 0
        return;
    const auto C0 = T0 ? T0->getParent() : 0;
    const auto C1 = T1 ? T1->getParent() : 0;
    auto &nav = getNavGrid();

    // 1. Allow routing inside the components (but not its pins or any tracks) if it is normally blocked.
    NavRasterizeParams rastC;
    rastC.IgnoreMask = NAV_POINT_FLAG_INSIDE_PIN;
    rastC.FlagsAnd = ~NAV_POINT_FLAG_BLOCKED_TEMPORARY;
    if (C0 && !C0->canRouteInside())
        nav.rasterize(*C0, rastC);
    if (C1 != C0 && C1 && !C1->canRouteInside())
        nav.rasterize(*C1, rastC);

    // 2. Allow tracks (but not vias) inside the two pins unconditionally, and remove the pin's clearance area.
    NavRasterizeParams rastE0;
    NavRasterizeParams rastE1;
    rastE0.FlagsAnd = ~NAV_POINT_FLAGS_TRACK_CLEARANCE; // remove track (but not via) clearance coming from the pin and any tracks
    rastE1.AutoExpand = true;
    rastE1.KOCount.setPinTracks(-1); // not vias: can't drill near the pin
    // IMPORTANT: must do E1 before E0 because E1 would restore TRACK_CLEARANCE where KO counts > 0
    nav.rasterizeCompound(*T0, rastE1);
    nav.rasterizeCompound(*T0, rastE0);
    if (T1 && !T1->inCompound(*T0)) {
        nav.rasterizeCompound(*T1, rastE1);
        nav.rasterizeCompound(*T1, rastE0);
    }

    if (T0 && T0->numConnections() != 1)
        for (auto Y : T0->connections())
            if (Y != &X && Y->hasTracks() && Y->getTrack(0).isRasterized())
                unrasterizeTracks(*Y, 0);
    if (T1 && T1->numConnections() != 1)
        for (auto Y : T1->connections())
            if (Y != &X && Y->hasTracks() && Y->getTrack(0).isRasterized())
                unrasterizeTracks(*Y, 0);
}
/// Undo the modifications of initPathfindingForNet(X).
void PCBoard::finiPathfindingForNet(const Connection &X)
{
    const Pin *T0 = X.sourcePin();
    const Pin *T1 = X.targetPin();
    assert(T0 != T1);
    if (T1 && !T0)
        std::swap(T0, T1);
    if (!T0) // T0 must be non-zero, T1 may be 0
        return;
    const auto C0 = T0 ? T0->getParent() : 0;
    const auto C1 = T1 ? T1->getParent() : 0;
    auto &nav = getNavGrid();

    // 1) Reset the blocked flag on the pins' parents if routing inside is prohibited.
    NavRasterizeParams rastC;
    rastC.IgnoreMask = NAV_POINT_FLAG_INSIDE_PIN; // don't set BLOCKED_TEMPORARY inside pins
    rastC.FlagsOr = NAV_POINT_FLAG_BLOCKED_TEMPORARY;
    if (C0 && !C0->canRouteInside())
        nav.rasterize(*C0, rastC);
    if (C1 != C0 && C1 && !C1->canRouteInside())
        nav.rasterize(*C1, rastC);

    // 2) Restore the clearance of the two pins.
    NavRasterizeParams rastP;
    rastP.AutoExpand = true;
    rastP.KOCount.setPinTracks(1);
    nav.rasterizeCompound(*T0, rastP);
    if (T1 && !T1->inCompound(*T0))
        nav.rasterizeCompound(*T1, rastP);

    if (T0 && T0->numConnections() != 1)
        for (auto Y : T0->connections())
            if (Y != &X && Y->hasTracks() && Y->getTrack(0).isRasterized())
                rasterizeTracks(*Y, 0);
    if (T1 && T1->numConnections() != 1)
        for (auto Y : T1->connections())
            if (Y != &X && Y->hasTracks() && Y->getTrack(0).isRasterized())
                rasterizeTracks(*Y, 0);
}

void PCBoard::initPathfindingFor(const Connection &X)
{
    if (X.hasNet())
        initPathfindingForNet(X);
    if (X.hasTracks() && X.getTrack(0).isRasterized())
        unrasterizeTracks(X);
}
void PCBoard::finiPathfindingFor(const Connection &X)
{
    if (X.hasNet())
        finiPathfindingForNet(X);
    // caller has to rasterize track
}

bool PCBoard::runPathFinding(Connection &X, Connection *ref, const AStarCosts *costs)
{
    getNavGrid().setSpacings(NavSpacings(X));
    if (!ref)
        ref = &X;
    // FIXME: Having to rasterize objects before routing is not very elegant.
    initPathfindingFor(*ref);
    auto rv = getNavGrid().findPathAStar(X, costs);
    finiPathfindingFor(*ref);
    return rv;
}

Real PCBoard::sumViolationArea(Connection &X, Connection *ref)
{
    getNavGrid().setSpacings(NavSpacings(X));
    if (!ref)
        ref = &X;
    initPathfindingFor(*ref);
    auto rv = getNavGrid().sumViolationArea(X);
    finiPathfindingFor(*ref);
    return rv;
}

void PCBoard::unrasterizeTracks(const Connection &X, int8_t count)
{
    NavRasterizeParams params;
    params.AutoExpand = true;
    params.KOCount.setRoute(-1);
    params.TrackRasterCount = count;
    getNavGrid().rasterize(X, params);
}
void PCBoard::eraseTracks(Connection &X)
{
    TRACE("eraseTracks " << X.str());
    if (X.isRasterized_allOrNone())
        unrasterizeTracks(X);
    X.clearTracks();
    setChanged(PCB_CHANGED_ROUTES | PCB_CHANGED_NAV_GRID);
}
void PCBoard::rasterizeTracks(const Connection &X, int8_t count)
{
    NavRasterizeParams params;
    params.AutoExpand = true;
    params.KOCount.setRoute(+1);
    params.TrackRasterCount = count;
    getNavGrid().rasterize(X, params);
    setChanged(PCB_CHANGED_ROUTES | PCB_CHANGED_NAV_GRID);
}

void PCBoard::wipe()
{
    for (const auto net : mNets)
        for (const auto X : net->connections())
            if (X->hasTracks())
                eraseTracks(*X);
}

void PCBoard::renumberComs()
{
    uint i = 0;
    for (auto C : getComponents())
        C->setId(i++);
    renumberPins();
}
void PCBoard::renumberPins()
{
    uint i = 0;
    for (auto C : getComponents())
        for (auto P : C->getPins())
            if (P->as<Pin>()->hasNet())
                P->setId(i++);
}
void PCBoard::renumberNets()
{
    uint i = 0;
    for (auto net : getNets())
        net->setId(i++);
}

PCBoard *PCBoard::clone(CloneEnv &env) const
{
    assert(&env.origin == this);
    PCBoard *PCB = new PCBoard();
    env.target = PCB;
    PCB->mLayers = mLayers;
    PCB->mLayoutArea = mLayoutArea;
    PCB->mName = mName;
    PCB->mTech = mTech;
    for (auto C : mParts)
        PCB->add(*dynamic_cast<Component *>(C->clone(env)));
    for (auto N : mNets)
        PCB->add(*N->clone(env));
    assert(PCB->mComponentAreaBbox == mComponentAreaBbox);
    for (uint i = 0; i < mParts.size(); ++i)
        assert(PCB->mParts[i]->id() == mParts[i]->id());
    PCB->mNavGrid.build();
    PCB->mNavGrid.copyFrom(mNavGrid);
    return PCB;
}

PyObject *PCBoard::getLayersPy() const
{
    auto py = PyList_New(mLayers.size());
    for (uint i = 0; i < mLayers.size(); ++i)
        PyList_SetItem(py, i, mLayers[i].getPy());
    return py;
}
PyObject *PCBoard::getPy(PyObject *_args, uint depth, bool asNumpy) const
{
    auto py = PyDict_New();
    if (!py)
        return 0;
    py::Object args(_args);
    if (args) {
        if (args.isLong()) {
            depth = args.asLong();
        } else if (args.isDict()) {
            auto d = args.item("depth");
            auto n = args.item("numpy");
            if (d.isLong())
                depth = d.asLong();
            if (n.isBool(false))
                asNumpy = false;
        }
    }
    py::Dict_StealItemString(py, "name", py::String(name()));
    py::Dict_StealItemString(py, "file", py::String(getSourceFilePath()));
    py::Dict_StealItemString(py, "resolution_nm", PyFloat_FromDouble(mUnitLength_nm.value()));
    py::Dict_StealItemString(py, "unit_nm", PyFloat_FromDouble(mUnitLength_nm.value()));
    py::Dict_StealItemString(py, "grid_size", *py::Object(mNavGrid.getSize()));
    py::Dict_StealItemString(py, "layout_area", *py::Object(mLayoutArea.bbox()));
    py::Dict_StealItemString(py, "layers", depth ? getLayersPy() : PyLong_FromLong(getNumLayers()));
    if (depth == 0)
        return py;
    auto nets = PyDict_New();
    auto coms = PyDict_New();
    py::Dict_StealItemString(py, "nets", nets);
    py::Dict_StealItemString(py, "components", coms);
    for (auto C : getComponents())
        py::Dict_StealItemString(coms, C->name().c_str(), C->getPy(depth - 1));
    for (auto N : getNets())
        py::Dict_StealItemString(nets, N->name().c_str(), N->getPy(depth - 1, asNumpy));
    return py;
}

void PCBoard::adjustLayoutAreaMargins(Real min, Real max)
{
    const auto &boxL = mLayoutArea.bbox();
    const auto &boxC = mComponentAreaBbox;
    const Real maxDim = std::max(boxC.xmax() - boxC.xmin(), boxC.ymax() - boxC.ymin());
    if (max < 0.0)
        max *= -maxDim;
    if (min < 0.0)
        min *= -maxDim;
    INFO("adjustLayoutAreaMargins: min=" << min << " max=" << max);
    if (min > max)
        throw std::invalid_argument("minimum > maximum layout area margin");
    mLayoutArea.setRect(Point_2(std::min(boxC.xmin() - min, std::max(boxL.xmin(), boxC.xmin() - max)),
                                std::min(boxC.ymin() - min, std::max(boxL.ymin(), boxC.ymin() - max))),
                        Point_2(std::max(boxC.xmax() + min, std::min(boxL.xmax(), boxC.xmax() + max)),
                                std::max(boxC.ymax() + min, std::min(boxL.ymax(), boxC.ymax() + max))));
}

Component *PCBoard::getComponent(PyObject *py) const
{
    if (!py::String_Check(py))
        throw std::invalid_argument("component identifier must be a string (name)");
    const auto name = py::String_AsStdString(py);
    auto C = getComponent(name);
    if (!C)
        throw std::runtime_error(fmt::format("component {} does not exist", name));
    return C;
}
Pin *PCBoard::getPin(PyObject *py) const
{
    std::string name;
    if (PyTuple_Check(py) && PyTuple_Size(py) == 2) {
        auto v0 = PyTuple_GetItem(py, 0);
        auto v1 = PyTuple_GetItem(py, 0);
        if (!py::String_Check(v0) || !py::String_Check(v1))
            throw std::invalid_argument("pin identifier tuple must consist of 2 strings");
        name = fmt::format("{}-{}", py::String_AsStringView(v0), py::String_AsStringView(v1));
    } else if (py::String_Check(py)) {
        name = py::String_AsStdString(py);
    } else {
        throw std::invalid_argument("pin identifier must be a string or tuple of 2 strings");
    }
    auto P = getPin(name);
    if (!P)
        throw std::runtime_error(fmt::format("pin {} does not exist", name));
    return P;
}
Net *PCBoard::getNet(PyObject *py) const
{
    if (!py::String_Check(py))
        throw std::invalid_argument("net identifier must be a string");
    const auto name = py::String_AsStdString(py);
    auto net = getNet(name);
    if (!net)
        throw std::runtime_error(fmt::format("net {} does not exist", name));
    return net;
}
Connection *PCBoard::getConnection(PyObject *py) const
{
    py::Object arg(py);
    if (!arg.isTupleOrList(2))
        throw std::invalid_argument("connection identifier must be a 2-tuple (net,int) or (pin,pin)");
    auto v0 = arg.isTuple() ? arg.elem(0) : arg.item(0u);
    auto v1 = arg.isTuple() ? arg.elem(1) : arg.item(1u);
    if (v1.isLong()) {
        auto net = getNet(*v0);
        if (!net)
            return 0;
        uint i = v1.asLong();
        if (i >= net->numConnections())
            throw std::runtime_error(fmt::format("connection #{} on net {} does not exist", i, net->name()));
        return net->connection(i);
    }
    auto P0 = getPin(*v0);
    if (!P0)
        return 0;
    auto P1 = getPin(*v1);
    if (!P1)
        return 0;
    Connection *X = (P0->hasNet() && P0->net() == P1->net()) ? P0->net()->getConnectionBetween(*P0, *P1) : 0;
    if (!X)
        throw std::runtime_error(fmt::format("no connection between pins {} and {}", P0->getFullName(), P1->getFullName()));
    return X;
}

void PCBoard::removeEmptyNets()
{
    std::vector<Net *> emptyNets;
    for (auto net : getNets())
        if (!net->numConnections())
            emptyNets.push_back(net);
    for (auto net : emptyNets)
        delete remove(*net);
    renumberNets();
}
void PCBoard::pruneConnections(std::set<Connection *> &keep, bool deleteNets)
{
    if (keep.empty())
        WARN("You are removing all connections");
    std::vector<Net *> emptyNets;
    for (auto net : getNets())
        if (net->pruneConnections(keep))
            emptyNets.push_back(net);
    if (!keep.empty())
        throw std::runtime_error("keep set contains connections that don't exist on this board");
    if (!deleteNets)
        return;
    for (auto net : emptyNets)
        delete remove(*net);
    renumberNets();
}
void PCBoard::pruneNets(std::set<Net *> &retain)
{
    if (retain.empty())
        WARN("You are removing all nets");
    std::vector<Net *> coffin;
    for (auto net : getNets())
        if (!retain.erase(net))
            coffin.push_back(net);
    for (auto net : coffin)
        delete remove(*net);
    if (!retain.empty())
        throw std::runtime_error("keep set contains nets that don't exist on this board");
    renumberNets();
}
void PCBoard::pruneComponents(std::set<Component *> &retain)
{
    if (retain.empty())
        WARN("You are removing all nets");
    std::vector<Component *> coffin;
    for (auto C : getComponents())
        if (!retain.erase(C))
            coffin.push_back(C);
    for (auto C : coffin)
        delete remove(*C);
    if (!retain.empty())
        throw std::runtime_error("keep set contains components that don't exist on this board");
    renumberComs();
    recomputeComponentAreaBbox();
    rebuildBVH();
}
void PCBoard::prunePins(std::set<Pin *> &retain)
{
    if (retain.empty())
        WARN("You are removing all nets");
    std::vector<Pin *> coffin;
    for (const auto C : getComponents())
        for (const auto P : C->getPins())
            if (!retain.erase(P->as<Pin>()))
                coffin.push_back(P->as<Pin>());
    for (auto P : coffin)
        delete P->getParent()->removeAndDisconnectPin(*P);
    if (!retain.empty())
        throw std::runtime_error("keep set contains pins that don't exist on this board");
    renumberPins();
    removeEmptyNets();
}

bool PCBoard::rebuildTNG()
{
    if (mTNG)
        delete mTNG;
    mTNG = NavTriangulation::create(*this);
    if (!mTNG)
        return false;
    return mTNG->build();
}
