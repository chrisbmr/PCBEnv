#ifndef GYM_PCB_RL_STATE_ENDPOINTS_H
#define GYM_PCB_RL_STATE_ENDPOINTS_H

#include "RL/StateRepresentation.hpp"

namespace sreps {

class ConnectionEndpoints : public StateRepresentation
{
public:
    ConnectionEndpoints(const std::vector<Connection *> * = 0);
    const char *name() const override { return "ends"; }
    void setFocus(const std::vector<Connection *> *) override;
    void reset() override;
    PyObject *getPy(PyObject *ignore) override;
private:
    py::ObjectRef mArray{0};
};

} // namespace sreps

#endif // GYM_PCB_RL_STATE_ENDPOINTS_H
