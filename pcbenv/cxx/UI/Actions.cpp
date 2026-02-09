
#include "Log.hpp"
#include "UI/Actions.hpp"
#include "UI/ActionTab.hpp"
#include "UI/Application.hpp"
#include "RL/Agent.hpp"
#include "RL/CommonActions.hpp"
#include "Component.hpp"
#include "Connection.hpp"
#include "Net.hpp"
#include "PCBoard.hpp"

UIActions::UIActions(UIApplication &A) : UIA(A)
{
    const auto &conf = UserSettings::get().UI;

    setPreview(conf.RoutePreview);
}

const std::vector<Connection *> *UIActions::getActiveConnections() const
{
    const auto A = UIA.getAutoAgent();
    if (A && A->getProgress() >= 0.0f && A->getProgress() < 1.0f)
        return &A->getConnections();
    return 0;
}

inline Point_25 UIActions::snap(const Connection *X, const Point_25 &v)
{
    if (X->targetPin() && X->targetPin()->contains3D(v))
        return X->target();
    if (X->sourcePin() && X->sourcePin()->contains3D(v))
        return X->source();
    for (const auto *T : X->getTracks()) {
        Point_25 vs;
        if (!T->empty() && T->snapToEndpoint(vs, v, 0.0))
            return vs;
    }
    if (mSnapToGrid)
        return UIA.getPCB()->getNavGrid().snapToMidPoint(v);
    return v;
}

/**
 * Depending on the direction of the selection, return either the source or the target point of the connection.
 * If a track has started on that point, return the endpoint of the track.
 */
Point_25 UIActions::getRouterStartPoint() const
{
    const auto *X = mSelection.X;
    if (!X)
        throw std::runtime_error("tried to get router start point but no connection selected");
    const bool e = X->targetPin() == mSelection.P[0];
    const auto P = e ? X->targetPin() : X->sourcePin();
    const auto E = e ? X->target() : X->source();
    for (const auto *T : X->getTracks())
        if (T->start().xy() == E.xy() && (P ? P->isOnLayer(T->start().z()) : T->start().z() == E.z()))
            return T->end();
    return E;
}

bool UIActions::_autoroute(PinSet pins)
{
    const auto A = UIA.getAutoAgent();
    if (A->getPCB() != UIA.getPCB())
        return UIA.endExclusiveTask(false, "Agent board does not match UI board");
    std::set<Connection *> conns;
    for (auto P : pins)
        conns.insert(P->connections().begin(), P->connections().end());
    A->setManagedConnections(conns);
    bool rv = false;
    try {
        rv = A->run();
    } catch (const std::exception &e) {
        ERROR(e.what());
    }
    return UIA.endExclusiveTask(rv);
}
bool UIActions::autoroute(uint64_t timeoutMS)
{
    if (!UIA.getAutoAgent()) {
        ERROR("No autorouter agent set. set_agent() must be called after render().");
        return false;
    }
    if (!UIA.tryStartExclusiveTask("autoroute"))
        return false;
    if (mAsyncRV.valid())
        mAsyncRV.wait();
    UIA.getAutoAgent()->setTimeout(timeoutMS * 1000);
    mAsyncRV = std::async(std::launch::async, &UIActions::_autoroute, this, mSelection.sets.P);
    return true;
}

bool UIActions::_connect(Connection *X)
{
    bool rv = false;
    try {
        actions::AStarConnect A(X);
        rv = A.performAs(*UIA.getUserAgent(), 0).Success;
    } catch (const std::exception &e) {
        ERROR(e.what());
    }
    return UIA.endExclusiveTask(rv);
}
bool UIActions::connect()
{
    const auto X = mSelection.X;
    if (!X || !UIA.tryStartExclusiveTask("connect"))
        return false;
    if (mAsyncRV.valid())
        mAsyncRV.wait();
    mAsyncRV = std::async(std::launch::async, &UIActions::_connect, this, X);
    return true;
}

bool UIActions::_routeToPoint(Connection *X, const Point_25 &v0, const Point_25 &v1)
{
    bool rv = false;
    try {
        actions::AStarToPoint A(X, v0, v1);
        rv = A.performAs(*UIA.getUserAgent(), 0).Success;
    } catch (const std::exception &e) {
        ERROR(e.what());
    }
    return UIA.endExclusiveTask(rv);
}
bool UIActions::routeToPoint(const Point_25 &_v)
{
    const auto X = mSelection.X;
    if (!X || X->isRouted())
        return false;
    const auto v1 = snap(X, _v);
    const auto v0 = getRouterStartPoint();
    if (v1 == v0 || !UIA.getPCB()->getLayoutArea().isInside(v1))
        return false;
    if (!UIA.tryStartExclusiveTask("routeToPoint"))
        return false;
    if (mAsyncRV.valid())
        mAsyncRV.wait();
    mAsyncRV = std::async(std::launch::async, &UIActions::_routeToPoint, this, X, v0, v1);
    return true;
}

bool UIActions::_routeToPointPreview(Connection *X, const Point_25 &v0, const Point_25 &v1)
{
    bool rv = false;
    try {
        const auto PCB = UIA.getPCB();
        Connection Y(*X, v0, v1);
        rv = PCB->runPathFinding(Y, X);
        if (X->hasTracks())
            PCB->rasterizeTracks(*X);
        if (rv)
            setPreviewTrack(Y.getTrack(0), true);
    } catch (const std::exception &e) {
        ERROR("_routeToPointPreview exception: " << e.what());
    }
    return UIA.endExclusiveTask(rv);
}
bool UIActions::routeToPointPreview(const Point_25 &_v)
{
    const auto X = mSelection.X;
    if (mPreviewReady || !X || X->isRouted())
        return false;
    const auto v1 = snap(X, _v);
    const auto v0 = getRouterStartPoint();
    if (v1 == v0) {
        setPreviewStale();
        return false;
    }
    const auto I = UIA.getPCB()->getNavGrid().LinearIndex(v1);
    if (mLastPreviewPoint == I || !UIA.getPCB()->getLayoutArea().isInside(v1))
        return false;
    if (!UIA.tryStartExclusiveTask("routeToPointPreview"))
        return false;
    mLastPreviewPoint = I;
    if (mAsyncRV.valid())
        mAsyncRV.wait();
    mAsyncRV = std::async(std::launch::async, &UIActions::_routeToPointPreview, this, X, v0, v1);
    return true;
}

bool UIActions::_addSegmentTo(Connection *X, const Point_25 &v0, const Point_25 &v1)
{
    bool rv = false;
    try {
        actions::SegmentToPoint A(X, v0, v1);
        rv = A.performAs(*UIA.getUserAgent(), 0).Success;
    } catch (const std::exception &e) {
        ERROR(e.what());
    }
    return UIA.endExclusiveTask(rv);
}
bool UIActions::addSegmentTo(const Point_25 &_v)
{
    const auto X = mSelection.X;
    if (!X || X->isRouted())
        return false;
    const auto v1 = snap(X, _v);
    const auto v0 = getRouterStartPoint();
    if (v1 == v0 || !UIA.getPCB()->getLayoutArea().isInside(v1))
        return false;
    if (!UIA.tryStartExclusiveTask("addSegmentTo"))
        return false;
    if (mAsyncRV.valid())
        mAsyncRV.wait();
    mAsyncRV = std::async(std::launch::async, &UIActions::_addSegmentTo, this, X, v0, v1);
    return true;
}

bool UIActions::_addSegmentToPreview(Connection *X, const Point_25 &v0, const Point_25 &v1)
{
    Connection Y(*X, v0, v1);
    Y.makeDirectTrack(MinSegmentLenSq, 0);
    const auto PCB = UIA.getPCB();
    Real AV = PCB->sumViolationArea(Y, X);
    if (X->hasTracks())
        PCB->rasterizeTracks(*X);
    setPreviewTrack(Y.getTrack(0), AV == 0.0);
    return UIA.endExclusiveTask(AV == 0.0);
}
bool UIActions::addSegmentToPreview(const Point_25 &_v)
{
    const auto X = mSelection.X;
    if (mPreviewReady || !X || X->isRouted())
        return false;
    const auto v1 = snap(X, _v);
    const auto v0 = getRouterStartPoint();
    const auto I = UIA.getPCB()->getNavGrid().LinearIndex(v1);
    if (v1 == v0 || mLastPreviewPoint == I || !UIA.getPCB()->getLayoutArea().isInside(v1))
        return false;
    if (!UIA.tryStartExclusiveTask("addSegmentToPreview"))
        return false;
    mLastPreviewPoint = I;
    if (mAsyncRV.valid())
        mAsyncRV.wait();
    mAsyncRV = std::async(std::launch::async, &UIActions::_addSegmentToPreview, this, X, v0, v1);
    return true;
}

bool UIActions::unrouteConnection()
{
    const auto X = mSelection.X;
    if (!X || !UIA.tryStartExclusiveTask("unrouteConnection"))
        return false;
    const auto PCB = UIA.getPCB();
    if (X->hasTracks())
        WITH_WLOCK(PCB, PCB->eraseTracks(*X));
    return UIA.endExclusiveTask(true);
}

bool UIActions::unrouteSegment()
{
    const auto X = mSelection.X;
    if (!X || !UIA.tryStartExclusiveTask("unrouteSegment"))
        return false;
    bool rv = false;
    try {
        actions::UnrouteSegment A(X, getRouterStartPoint());
        rv = A.performAs(*UIA.getUserAgent(), 0).Success;
    } catch (const std::exception &e) {
        ERROR(e.what());
    }
    return UIA.endExclusiveTask(rv);
}

bool UIActions::unrouteSelection()
{
    if (!UIA.tryStartExclusiveTask("unrouteSelection"))
        return false;
    std::set<Connection *> Xs;
    const auto PCB = UIA.getPCB();
    for (auto C : mSelection.sets.C)
        for (auto A : C->getPins())
            if (auto P = A->as<Pin>())
                Xs.insert(P->connections().begin(), P->connections().end());
    for (auto P : mSelection.sets.P)
        Xs.insert(P->connections().begin(), P->connections().end());
    std::lock_guard wlock(PCB->getLock());
    for (auto X : Xs)
        if (X->hasTracks())
            PCB->eraseTracks(*X);
    return UIA.endExclusiveTask(true);
}

void UIActions::updateIndicators()
{
    auto A = UIA.getAutoAgent();
    if (A && mActionTab)
        mActionTab->setProgress(std::max(0.0f, A->getProgress()) * 100);
}

void UIActions::interrupt()
{
    auto A = UIA.getAutoAgent();
    if (A)
        A->setTimeoutExpired();
}

bool UIActions::routeTo(const Point_2 &v2)
{
    const auto v = Point_25(v2, mActiveLayer);
    if (mRouteMode == '*')
        return routeToPoint(v);
    else if (mRouteMode == 's')
        return addSegmentTo(v);
    return false;
}

bool UIActions::routePreview(const Point_2 &v2)
{
    const auto v = Point_25(v2, mActiveLayer);
    if (mRouteMode == '*')
        return routeToPointPreview(v);
    else if (mRouteMode == 's')
        return addSegmentToPreview(v);
    return false;
}

void UIActions::setRouteMode(char mode)
{
    if (mRouteMode != mode)
        setPreviewStale();
    mRouteMode = mode;
    if (mActionTab)
        mActionTab->setActiveTool(mode);
    if (mRouteMode)
        mLastRouteMode = mRouteMode;
}

void UIActions::setActiveLayer(uint z)
{
    if (mActiveLayer != z) {
        mActiveLayer = z;
        UIA.getPCB()->setChanged(PCB_CHANGED_NAV_GRID);
    }
}

void UIActions::setSelection(Object *A)
{
    if (auto C = dynamic_cast<Component *>(A))
        setSelection(C);
    else if (auto P = dynamic_cast<Pin *>(A))
        setSelection(P);
    INFO("Selected " << (A ? A->getFullName() : "null"));
}

void UIActions::setSelection(Component *C)
{
    mSelection.C = C;
}

void UIActions::setSelection(Pin *P)
{
    setSelectionNoNotify(P);
    if (mActionTab)
        mActionTab->setPins(mSelection.P[0], mSelection.P[1]);
}
void UIActions::setSelectionNoNotify(Pin *P)
{
    setPreviewStale();
    // A,B     means A -> B
    // A,B,B   means B -> A
    // A,B,A   means B -> A
    // A,B,A,A means A -> B
    // A,B,C   means B -> C
    // A,B,C,A means C -> A
    // A,B,C,C means C -> B
    // A,B,C,B means C -> B
    if (!mSelection.P[0]) {
        mSelection.P[0] = P;
    } else if (!mSelection.P[1]) {
        if (P != mSelection.P[0])
            mSelection.P[1] = P;
    } else if (P == mSelection.P[mSelection.P0or1]) {
        std::swap(mSelection.P[0], mSelection.P[1]);
    } else {
        mSelection.P[0] = mSelection.P[mSelection.P0or1];
        mSelection.P[1] = P;
    }
    mSelection.P0or1 = (mSelection.P[1] == P) ? 1 : 0;
    // If this pin has exactly 1 connection, select the other pin automatically.
    Pin *P2 = 0;
    if (P->numConnections() == 1)
        P2 = (*P->connections().begin())->otherPin(P);
    if (P2 && mSelection.P[0] != P2 && mSelection.P[1] != P2) {
        mSelection.P[0] = P;
        mSelection.P[1] = P2;
        mSelection.P0or1 = 0;
    }
    setNetAndConnectionFromPins();
}
void UIActions::setNetAndConnectionFromPins()
{
    auto P0 = mSelection.P[0];
    auto P1 = mSelection.P[1];
    mSelection.net = 0;
    mSelection.X = 0;
    if (!P0 || !P1 || !P0->net() || P0->net() != P1->net())
        return;
    mSelection.net = P0->net();
    mSelection.X = P0->net()->getConnectionBetween(*P0, *P1);
}

void UIActions::setSelection(const std::set<Object *> &set)
{
    mSelection.sets.C.clear();
    mSelection.sets.P.clear();    
    for (auto I : set) {
        if (auto C = I->as<Component>())
            mSelection.sets.C.insert(C);
        else if (auto P = I->as<Pin>())
            mSelection.sets.P.insert(P);
    }
    if (mActionTab)
        mActionTab->newSelections();
}

void UIActions::deselectAll()
{
    mSelection.sets.C.clear();
    mSelection.sets.P.clear();
    if (mActionTab)
        mActionTab->newSelections();
}

void UIActions::reset()
{
    deselectAll();
    mSelection.C = 0;
    mSelection.P[0] = mSelection.P[1] = 0;
    mSelection.net = 0;
    mSelection.X = 0;
    mSelection.P0or1 = 0;
    if (mActionTab) {
        mActionTab->setPins(0, 0);
        mActionTab->newSelections();
    }
    resetLastPreviewPoint();
}

void UIActions::deselect(const Component *C)
{
    mSelection.sets.C.erase(const_cast<Component *>(C));
    if (mActionTab)
        mActionTab->newSelections();
}
void UIActions::deselect(const Pin *P)
{
    mSelection.sets.P.erase(const_cast<Pin *>(P));
    if (mActionTab)
        mActionTab->newSelections();
}

void UIActions::setLockStepping(uint gran)
{
    INFO("Lock-stepping " << (gran ? "ON": "OFF") << '(' << gran << ')');
    if (UIA.getUserAgent())
        UIA.getUserAgent()->getStepLock().setGranularity(gran);
    if (UIA.getAutoAgent())
        UIA.getAutoAgent()->getStepLock().setGranularity(gran);
}
void UIActions::nextStep()
{
    INFO("STEP");
    if (UIA.getUserAgent())
        UIA.getUserAgent()->getStepLock().signal();
    if (UIA.getAutoAgent())
        UIA.getAutoAgent()->getStepLock().signal();
}

void UIActions::setViaCost(float v)
{
    UserSettings::edit().AStarViaCostFactor = v;
    mViaCost = v;
    if (UIA.getPCB())
        UIA.getPCB()->getNavGrid().getAStarCosts().Via = v;
}
