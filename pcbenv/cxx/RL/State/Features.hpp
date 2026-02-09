#ifndef GYM_PCB_RL_STATE_FEATURES_H
#define GYM_PCB_RL_STATE_FEATURES_H

#include "RL/StateRepresentation.hpp"

namespace sreps {

class CustomFeatures : public StateRepresentation
{
public:
    const char *name() const override { return "features"; }
    void init(PCBoard&) override;
    void setFocus(const std::vector<Connection *> *) override;
    PyObject *getPy(PyObject *args) override;
    PyObject *getArray();
    PyObject *getDict();
private:
    std::vector<Connection *> mConnections;
};

} // namespace sreps

#endif // GYM_PCB_RL_STATE_FEATURES_H
