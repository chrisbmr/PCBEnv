
#ifndef GYM_PCB_UI_ACTIONS_H
#define GYM_PCB_UI_ACTIONS_H

#include <future>
#include <set>
#include "UserSettings.hpp"
#include "Object.hpp"
#include "Track.hpp"

class UIApplication;

class ActionTab;
class Component;
class Pin;
class Net;
class Connection;

class ObjectPtrNameLessThan
{
public:
    /// Parent names take precedent, no parent counts as less.
    bool operator()(const Object *L, const Object *R) const
    {
        assert(L && R);
        if (L->getParent() == R->getParent())
            return L->name() < R->name();
        if (L->getParent() && R->getParent())
            return L->getParent()->name() < R->getParent()->name();
        return !L->getParent();
    }
};
using ComSet = std::set<Component *, ObjectPtrNameLessThan>; 
using PinSet = std::set<Pin *, ObjectPtrNameLessThan>; 


/// This class implements actions on the board that can be triggered by the user
/// and contains related state.
/// The _ functions are run in threads via std::async.

/// WARNING: The non-async UIActions functions are expected to be non-concurrent.

/// FIXME: In principle, Python code could delete objects that we are referencing
/// without notifying us. Just don't mix UI actions and scripted ones like that!
/// Otherwise we have to add more locking, shared_ptr, and discard flags.
/// Typically you won't want to delete components and pins on the fly anyway.
class UIActions
{
public:
    constexpr static const Real MinSegmentLenSq = 1.0;
public:
    UIActions(UIApplication&);
    void setUIElement(ActionTab *);
    void reset();
    Connection *getConnection() const { return mSelection.X; }
    const auto& getSelectionSetC() const { return mSelection.sets.C; }
    const auto& getSelectionSetP() const { return mSelection.sets.P; }
    const std::vector<Connection *> *getActiveConnections() const;
    void deselectAll();
    void deselect(const Component *);
    void deselect(const Pin *);
    void setSelection(Object *);
    void setSelection(Component *);
    void setSelection(Pin *);
    void setSelection(const std::set<Object *>&);
    void setSelectionLayer(char); // 'V'/'A'/'W'/'T'/'B' for view/all/working/top/bottom
    char getSelectionLayer() const;
    void setPreview(bool);
    void setPreviewTrack(const Track&, bool legal);
    bool isPreviewing() const;
    bool isPreviewReady() const;
    bool isPreviewStale() const;
    bool isPreviewTrackLegal() const;
    const Track& getRoutePreview() const;
    void discardPreview();
    void setTimeoutMS(uint64_t);
    void setActiveLayer(uint);
    uint getActiveLayer() const;
    void setViaCost(float);
    bool autoroute(uint64_t timeoutMS);
    bool connect();
    bool routeToPoint(const Point_25&);
    bool addSegmentTo(const Point_25&);
    bool routeTo(const Point_2&);
    void setRouteMode(char mode); // 0, '*', or 's' for none, A*, segment
    char getRouteMode() const;
    char getLastRouteMode() const;
    bool routePreview(const Point_2&);
    bool routeToPointPreview(const Point_25&);
    bool addSegmentToPreview(const Point_25&);
    bool unrouteConnection();
    bool unrouteSegment();
    bool unrouteSelection();
    void updateIndicators();
    void interrupt();
    void setLockStepping(uint granularity);
    void nextStep();
    void setSelectionType(char c); // 'P'/'N'/'C' for pins/nets/components
    char getSelectionType() const;
private:
    ActionTab *mActionTab{0};
    uint mActiveLayer{0};
    char mRouteMode{0};
    char mLastRouteMode{'*'};
    bool mSnapToGrid{true};
    bool mPreview{false};
    bool mPreviewReady{false};
    bool mPreviewStale{false};
    bool mPreviewLegal{false};
    uint mLastPreviewPoint{std::numeric_limits<uint>::max()};
    Track mPreviewTrack{Point_25(0,0,0)};
    struct {
        Component *C{0};
        Pin *P[2]{0,0};
        Net *net{0};
        Connection *X{0};
        uint8_t P0or1;
        struct {
            ComSet C;
            PinSet P;
        } sets;
        char type{'P'};
        char layer{'V'};
    } mSelection;
    float mViaCost{1.0f};
    void setNetAndConnectionFromPins();
    void resetLastPreviewPoint();
    void setPreviewStale();
    void setSelectionNoNotify(Pin *);
private:
    bool _autoroute(PinSet);
    bool _routeToPoint(Connection *, const Point_25&, const Point_25&);
    bool _addSegmentTo(Connection *, const Point_25&, const Point_25&);
    bool _routeToPointPreview(Connection *, const Point_25&, const Point_25&);
    bool _addSegmentToPreview(Connection *, const Point_25&, const Point_25&);
    bool _connect(Connection *);

    Point_25 snap(const Connection *, const Point_25&);
    Point_25 getRouterStartPoint() const;
private:
    UIApplication &UIA;
    std::future<bool> mAsyncRV;
    uint64_t mTimeoutMS;
};

inline void UIActions::setUIElement(ActionTab *A)
{
    mActionTab = A;
}
inline void UIActions::setPreview(bool b)
{
    mPreview = b;
    if (!b)
        setPreviewStale();
}
inline void UIActions::setPreviewTrack(const Track &T, bool legal)
{
    mPreviewTrack = T;
    mPreviewReady = true;
    mPreviewStale = false;
    mPreviewLegal = legal;
}
inline bool UIActions::isPreviewing() const
{
    return mPreview;
}
inline bool UIActions::isPreviewReady() const
{
    return mPreviewReady;
}
inline bool UIActions::isPreviewStale() const
{
    return mPreviewStale;
}
inline bool UIActions::isPreviewTrackLegal() const
{
    return mPreviewLegal;
}
inline const Track& UIActions::getRoutePreview() const
{
    return mPreviewTrack;
}
inline void UIActions::discardPreview()
{
    mPreviewTrack.clear();
    mPreviewReady = false;
}
inline void UIActions::setTimeoutMS(uint64_t ms)
{
    mTimeoutMS = ms;
}
inline char UIActions::getRouteMode() const
{
    return mRouteMode;
}
inline char UIActions::getLastRouteMode() const
{
    return mLastRouteMode;
}
inline uint UIActions::getActiveLayer() const
{
    return mActiveLayer;
}
inline void UIActions::setSelectionLayer(char c)
{
    assert(c == 'A' || c == 'V' || c == 'W' || c == 'T' || c == 'B');
    mSelection.layer = c;
}
inline char UIActions::getSelectionLayer() const
{
    return mSelection.layer;
}
inline void UIActions::setSelectionType(char c)
{
    assert(c == 'P' || c == 'N' || c == 'C');
    mSelection.type = c;
}
inline char UIActions::getSelectionType() const
{
    return mSelection.type;
}

inline void UIActions::resetLastPreviewPoint()
{
    mLastPreviewPoint = std::numeric_limits<uint>::max();
}

inline void UIActions::setPreviewStale()
{
    mPreviewStale = true;
    resetLastPreviewPoint();
}

#endif // GYM_PCB_UI_ACTIONS_H
