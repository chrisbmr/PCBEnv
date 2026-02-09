#ifndef GYM_PCB_RL_STATE_ITEMS_H
#define GYM_PCB_RL_STATE_ITEMS_H

#include "RL/StateRepresentation.hpp"

namespace sreps {

class WholeBoard : public StateRepresentation
{
public:
    const char *name() const override { return "board"; }
    PyObject *getPy(PyObject *ignore) override;
};

class ItemSelection : public StateRepresentation
{
public:
    const char *name() const override { return "select"; }
    PyObject *getPy(PyObject *args) override;
private:
    void selectNets(PyObject *dest, PyObject *args);
};

} // namespace sreps

#endif // GYM_PCB_RL_STATE_ITEMS_H
