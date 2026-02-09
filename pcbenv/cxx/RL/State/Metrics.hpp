#ifndef GYM_PCB_RL_STATE_METRICS_H
#define GYM_PCB_RL_STATE_METRICS_H

#include "RL/StateRepresentation.hpp"

namespace sreps {

class Metrics : public StateRepresentation
{
public:
    const char *name() const override { return "metric"; }
    PyObject *getPy(PyObject *name) override;
};

} // namespace sreps

#endif // GYM_PCB_RL_STATE_METRICS_H
