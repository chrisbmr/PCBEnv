#ifndef GYM_PCB_RL_ACTIONS_LAYERMASK_H
#define GYM_PCB_RL_ACTIONS_LAYERMASK_H

#include "RL/Action.hpp"

namespace actions {

/// Must be a tuple(net, integer mask)
class SetLayerMask final : public Action
{
public:
    SetLayerMask() : Action("set_layer_mask") { }
    Result performAs(Agent&, PyObject *) override;
};

} // namespace actions

#endif // GYM_PCB_RL_ACTIONS_LAYERMASK_H
