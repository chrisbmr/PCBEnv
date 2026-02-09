#ifndef GYM_PCB_RL_ACTIONS_ADDSEGMENT_H
#define GYM_PCB_RL_ACTIONS_ADDSEGMENT_H

#include "RL/Actions/Route.hpp"

namespace actions {

class SegmentToPoint final : public RouteToAction
{
public:
    constexpr static const Real MinSegmentLenSq = 1.0;
    SegmentToPoint(Connection *Y, const Point_25 &A, const Point_25 &B) : RouteToAction("segment_to", Y, A, B) { }
    Result performAs(Agent&, PyObject *) override;
};

} // namespace actions

#endif // GYM_PCB_RL_ACTIONS_ADDSEGMENT_H
