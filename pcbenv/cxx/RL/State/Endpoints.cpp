#include "PyArray.hpp"
#include "RL/State/Endpoints.hpp"
#include "Connection.hpp"

namespace sreps {

ConnectionEndpoints::ConnectionEndpoints(const std::vector<Connection *> *V)
{
    if (V)
        setFocus(V);
}
void ConnectionEndpoints::setFocus(const std::vector<Connection *> *V)
{
    if (!V) {
        reset();
        return;
    }
    float *const coords = (float *)std::malloc(V->size() * 6 * sizeof(float));
    for (uint i = 0; i < V->size(); ++i) {
        const auto X = (*V)[i];
        coords[i * 6 + 0] = X->source().x();
        coords[i * 6 + 1] = X->source().y();
        coords[i * 6 + 2] = X->source().z();
        coords[i * 6 + 3] = X->target().x();
        coords[i * 6 + 4] = X->target().y();
        coords[i * 6 + 5] = X->target().z();
    }
    mArray.reset(py::NPArray<float>(coords, V->size(), 6).ownData().release());
}
void ConnectionEndpoints::reset()
{
    mArray.reset(0);
}
PyObject *ConnectionEndpoints::getPy(PyObject *ignore)
{
    return mArray.getRef();
}

} // namespace sreps
