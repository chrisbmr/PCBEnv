
#ifndef GYM_PCB_LOADERS_ROUTETRACKER_H
#define GYM_PCB_LOADERS_ROUTETRACKER_H

#include "AShape.hpp"
#include "Via.hpp"
#include "Loaders/Factory.hpp"

class Net;
class Pin;

class RouteTracker
{
public:
    RouteTracker(Net&, const std::vector<WideSegment_25>&, const std::vector<Via>&);
    bool run();
private:
    Net &mNet;
    const std::vector<WideSegment_25> &mSegs;
    const std::vector<Via> &mVias;
    std::map<Point_25, std::set<const Via *>> mPoint2Vias;
    std::vector<std::list<WideSegment_25>> mTracks;
    std::map<Point_2, std::set<uint>> mPoint2Tracks;
    std::map<Point_25, Pin *> mPt2Pin;
    void stitchTracks();
    void mergeTracks(uint t, uint u);
    void eraseFront(uint t);
    void eraseBack(uint t);
    void erasedFromTrack(const WideSegment_25&, uint t);
    uint tryAppend(std::set<uint>&, const WideSegment_25&, uint except = std::numeric_limits<uint>::max());
    bool tryAppend(uint track, const WideSegment_25&);
    bool tryConnectTracks(uint t, uint u);
    bool canAddVia(const Point_25 &v, int z) const;
    void makeConnections();
    void addConnection(Pin *, std::vector<WideSegment_25>&, std::vector<Via>&, Pin *);
    Pin *getPin(const Point_25&) const;
};
inline Pin *RouteTracker::getPin(const Point_25 &v) const
{
    auto I = mPt2Pin.find(v);
    if (I != mPt2Pin.end())
        return I->second;
    return 0;
}

#endif // GYM_PCB_LOADERS_ROUTETRACKER_H
