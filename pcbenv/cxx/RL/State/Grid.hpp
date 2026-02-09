#ifndef GYM_PCB_RL_STATE_GRID_H
#define GYM_PCB_RL_STATE_GRID_H

#include "RL/StateRepresentation.hpp"

namespace sreps {

class GridData : public StateRepresentation
{
public:
    void init(PCBoard&) override;
    void setBox(const IBox_3 &box) { mBox = box; }
    const char *name() const override { return "grid"; }
    PyObject *getPy(PyObject *members_and_box) override;
private:
    IBox_3 mBox;
};

} // namespace sreps

#endif // GYM_PCB_RL_STATE_GRID_H
