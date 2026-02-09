
#ifndef GYM_PCB_RL_USERAGENT_H
#define GYM_PCB_RL_USERAGENT_H

#include "RL/ActionSpace.hpp"
#include "RL/Agent.hpp"
#include "RL/CommonActions.hpp"
#include "RL/CommonStateReprs.hpp"
#include "RL/Reward.hpp"

/**
 * Implementation of the default agent that just implements step() and get_state() for Python control.
 */
class UserAgent : public Agent
{
public:
    UserAgent();
    void setPCB(const std::shared_ptr<PCBoard>&) override;
    void setManagedConnections(const std::set<Connection *>&) override;
    void setStateRepresentationParams(PyObject *) override;
    void setParameter(const std::string &name, int index, PyObject *) override;
    PyObject *step(PyObject *) override;
    PyObject *get_state(PyObject *) override;
    PyObject *reset() override;
private:
    struct {
        actions::AStarConnect Connect{0};
        actions::SetRouteGuard RouteGuard;
        actions::SetLayerMask LayerMask;
        actions::SetCostMap CostMap;
        actions::AStarToPoint RouteTo{0, Point_25(0,0,-1), Point_25(0,0,-1)};
        actions::SegmentToPoint SegmentTo{0, Point_25(0,0,-1), Point_25(0,0,-1)};
        actions::Unroute Unroute{0};
        actions::UnrouteSegment UnrouteSegment{0, Point_25(0,0,-1)};
        actions::UnrouteNet UnrouteNet{0};
        actions::SetTrack SetTrack{0};
        actions::LockRouted Lock;
    } mActions;
    ActionSpace mActionSpaceU;
    ActionSpace mActionSpaceLegacy;
    struct {
        std::unique_ptr<sreps::Image> DImage;
        std::unique_ptr<sreps::Image> GImage;
        StateRepresentation None;
        sreps::WholeBoard Board;
        sreps::GridData Grid;
        sreps::ConnectionEndpoints EndpointsNumpy;
        sreps::TrackRasterization Raster;
        sreps::TrackSegments Segments{true};
        sreps::Metrics Metrics;
        sreps::CustomFeatures Features;
        sreps::ItemSelection Selection;
        sreps::ClearanceCheck Clearance;
        std::map<std::string, StateRepresentation *> map;
        StateRepresentation *def;
    } mSR;
    Reward mLastReward;
    RouterResult mRouterResult;
    IBox_3 mGridSRBox{IBox_3::EMPTY()};
    IVector_2 mImageSize{128, 128};

    Action::Result _action(PyObject *);
    void initActionsUser();
    void initActionsLegacy();
    void initSR();
    void loadAllConnections();

    PyObject *_get_state(PyObject *) const;
};

#endif // GYM_PCB_RL_USERAGENT_H
