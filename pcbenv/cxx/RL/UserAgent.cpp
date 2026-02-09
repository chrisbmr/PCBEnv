
#include "Log.hpp"
#include "PyArray.hpp"
#include "RL/UserAgent.hpp"
#include "PCBoard.hpp"
#include "NavImage.hpp"
#include "Net.hpp"

UserAgent::UserAgent() : Agent("user")
{
    mSR.def = &mSR.None;
    mSR.map[mSR.None.name()] = &mSR.None;
    mSR.map[mSR.Board.name()] = &mSR.Board;
    mSR.map[mSR.EndpointsNumpy.name()] = &mSR.EndpointsNumpy;
    mSR.map[mSR.Grid.name()] = &mSR.Grid;
    mSR.map[mSR.Raster.name()] = &mSR.Raster;
    mSR.map[mSR.Segments.name()] = &mSR.Segments;
    mSR.map[mSR.Metrics.name()] = &mSR.Metrics;
    mSR.map[mSR.Features.name()] = &mSR.Features;
    mSR.map[mSR.Selection.name()] = &mSR.Selection;
    mSR.map[mSR.Clearance.name()] = &mSR.Clearance;
    mRewardFn.reset(new rewards::RouteLength());
    mLastReward = 0.0f;
}

void UserAgent::initActionsUser()
{
    if (mActionSpaceU.size())
        return;
    mActionSpaceU.addAction(&mActions.Connect);
    mActionSpaceU.addAction(&mActions.RouteGuard);
    mActionSpaceU.addAction(&mActions.LayerMask);
    mActionSpaceU.addAction(&mActions.CostMap);
    mActionSpaceU.addAction(&mActions.RouteTo);
    mActionSpaceU.addAction(&mActions.SegmentTo);
    mActionSpaceU.addAction(&mActions.Unroute);
    mActionSpaceU.addAction(&mActions.UnrouteNet);
    mActionSpaceU.addAction(&mActions.UnrouteSegment);
    mActionSpaceU.addAction(&mActions.SetTrack);
    mActionSpaceU.addAction(&mActions.Lock);
}
void UserAgent::initActionsLegacy()
{
    mActionSpaceLegacy.clear(true);
    mActionSpaceLegacy.setOwnsActions(true);
    for (auto X : getConnections())
        mActionSpaceLegacy.addAction(new actions::AStarConnect(X, fmt::format("astar({})", X->name())));
}
void UserAgent::initSR()
{
    const auto def = mSR.def->name();
    mSR.DImage.reset(sreps::Image::createRasterize(mImageSize.x, mImageSize.y));
    mSR.GImage.reset(sreps::Image::createDownscale(mImageSize.x, mImageSize.y));
    mSR.map[mSR.DImage->name()] = mSR.DImage.get();
    mSR.map[mSR.GImage->name()] = mSR.GImage.get();
    mSR.def = mSR.map[def];
    if (!mPCB)
        return;
    for (auto I : mSR.map)
        I.second->init(*mPCB);
    if (mGridSRBox.valid())
        mSR.Grid.setBox(mGridSRBox);
}
void UserAgent::loadAllConnections()
{
    std::set<Connection *> conns;
    if (mPCB)
        for (const auto net : mPCB->getNets())
            for (Connection *X : net->connections())
                if (!X->isLocked())
                    conns.insert(X);
    setManagedConnections(conns);
}

/**
 * Set the default parameters for some state representations.
 */
void UserAgent::setStateRepresentationParams(PyObject *py)
{
    py::Object param(py);

    if (!param.isDict())
        throw std::invalid_argument("state_representation must be a dict");

    if (auto def = param.item("default")) {
        const auto name = def.asString("state_representation['default'] must be a string");
        const auto I = mSR.map.find(name);
        if (I == mSR.map.end())
            throw std::invalid_argument(fmt::format("unknown state representation: {}", name));
        mSR.def = I->second;
    }

    if (auto image_size = param.item("image_size"))
        mImageSize = image_size.asIVector_2("image size must be an integer 2-tuple");

    if (auto grid_box = param.item("grid_box"))
        mGridSRBox = grid_box.asIBox_3();
}
void UserAgent::setParameter(const std::string &name, int index, PyObject *py)
{
    if (name == "srep")
        setStateRepresentationParams(py);
    else
        Agent::setParameter(name, index, py);
}

void UserAgent::setPCB(const std::shared_ptr<PCBoard> &PCB)
{
    Agent::setPCB(PCB);
    initSR();
    loadAllConnections();
    initActionsUser();
    initActionsLegacy();
}

void UserAgent::setManagedConnections(const std::set<Connection *> &X)
{
    Agent::setManagedConnections(X);
    initActionsLegacy();
    mSR.Features.setFocus(&mConnections);
    mSR.EndpointsNumpy.setFocus(&mConnections);
}

PyObject *UserAgent::reset()
{
    py::GIL_guard GIL;
    for (auto X : getConnections()) {
        if (X->isLocked())
            continue;
        mActions.Unroute.setConnection(X);
        mActions.Unroute.performAs(*this, 0);
    }
    resetActionCount();
    mLastReward = 0.0f;
    return _get_state(0);
}

Action::Result UserAgent::_action(PyObject *arg)
{
    Action::Result rv;
    try {
        if (PyLong_Check(arg)) {
            Action *A = mActionSpaceLegacy.getAction(PyLong_AsLong(arg));
            if (A) {
                rv = A->performAs(*this, 0);
            } else {
                PyErr_SetString(PyExc_IndexError, "action index is out of range");
            }
        } else if (PyTuple_Check(arg) && PyTuple_Size(arg) == 2) {
            auto name = PyTuple_GetItem(arg, 0);
            auto args = PyTuple_GetItem(arg, 1);
            Action *A = py::String_Check(name) ? mActionSpaceU.getAction(py::String_AsStdString(name)) : 0;
            if (A)
                rv = A->performAs(*this, args);
            else
                PyErr_SetString(PyExc_ValueError, "action name is not a known string");
        } else {
            PyErr_SetString(PyExc_ValueError, "expected action to be an integer or a tuple(name, args)");
        }
    } catch (std::exception &e) {
        PyErr_SetString(PyExc_Exception, e.what());
    }
    return rv;
}

/**
 * Execute a user action.
 * This catches C++ exceptions and turns them into Python exceptions.
 * @return (state, reward, success, termination[, track information])
 */
PyObject *UserAgent::step(PyObject *arg)
{
    // FIXME: We hold the GIL during the whole action which is not ideal
    py::GIL_guard GIL;
    if (!mPCB)
        return py::Exception(PyExc_RuntimeError, "step() called without board");
    Action::Result res = _action(arg);
    if (PyErr_Occurred())
        return 0;
    mSR.def->setFocus(getConnectionLRU());
    mLastReward = res.R;
    auto rv = PyTuple_New(res.Router.isSet() ? 5 : 4);
    PyTuple_SetItem(rv, 0, _get_state(0));
    PyTuple_SetItem(rv, 1, PyFloat_FromDouble(mLastReward));
    PyTuple_SetItem(rv, 2, PyBool_FromLong(res.Success));
    PyTuple_SetItem(rv, 3, PyBool_FromLong(res.Termination));
    if (res.Router.isSet())
        PyTuple_SetItem(rv, 4, res.Router.getPy());
    return rv;
}

/**
 * Obtain a specific state representation.
 * This catches C++ exceptions and turns them into Python exceptions.
 * @return depends on the parameters
 */
PyObject *UserAgent::_get_state(PyObject *req) const
{
    if (!mPCB)
        return py::Exception(PyExc_RuntimeError, "get_state() called without board");
    if (!req)
        return mSR.def->getPy(0);
    if (req == Py_None)
        return py::None();
    if (!PyDict_Check(req))
        return py::ValueError("get_state argument must be a dict");
    PyObject *res = PyDict_New();
    try {
        py::DictIterator I(req);
        while (I.next())
            py::Dict_StealItem(res, *I.k, mSR.map.at(I.k.asString())->getPy(*I.v));
    } catch (const std::exception &e) {
        py::Exception(e);
    }
    if (PyErr_Occurred())
        Py_DECREF(res);
    return PyErr_Occurred() ? 0 : res;
}
PyObject *UserAgent::get_state(PyObject *req)
{
    py::GIL_guard GIL;
    return _get_state(req);
}
