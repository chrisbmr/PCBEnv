#include "RL/State/Tracks.hpp"
#include "PCBoard.hpp"
#include "Connection.hpp"

namespace sreps {

inline PyObject *TrackRasterization::getPy1(const Connection &X) const
{
    auto py = PyList_New(X.numTracks());
    for (uint i = 0; i < X.numTracks(); ++i)
        PyList_SetItem(py, i, mPCB->getNavGrid().getPathCoordinatesNumpy(X.getTrack(i)));
    return py;
}
inline PyObject *TrackRasterization::getPy1(PyObject *arg) const
{
    return getPy1(*mPCB->getConnection(arg));
}
PyObject *TrackRasterization::getPy(PyObject *args)
{
    assert(mPCB);
    if (!mPCB)
        return 0;
    if (args && PyList_Check(args)) {
        const uint N = PyList_Size(args);
        PyObject *rv = PyList_New(N);
        for (uint i = 0; i < N; ++i)
            PyList_SetItem(rv, i, getPy1(PyList_GetItem(args, i)));
        return rv;
    }
    const Connection *X = args ? mPCB->getConnection(args) : mFocus;
    if (!X)
        return py::None(); // throw std::runtime_error("no track specified for state (track rasterization)");
    return getPy1(*X);
}

PyObject *TrackSegments::getPy(PyObject *args)
{
    assert(mPCB);
    if (!mPCB)
        return 0;
    if (args && PyList_Check(args)) {
        const uint N = PyList_Size(args);
        PyObject *rv = PyList_New(N);
        for (uint i = 0; i < N; ++i)
            PyList_SetItem(rv, i, mPCB->getConnection(PyList_GetItem(args, i))->getTracksPy(mFormatNumpy));
        return rv;
    }
    const Connection *X = args ? mPCB->getConnection(args) : mFocus;
    if (!X)
        return py::None(); // throw std::runtime_error("no track specified for state (track segments)");
    return X->getTracksPy(mFormatNumpy);
}

} // namespace sreps
