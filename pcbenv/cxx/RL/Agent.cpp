
#include "PyArray.hpp"
#include "Log.hpp"
#include "UserSettings.hpp"
#include "RL/Agent.hpp"
#include "RL/RRRAgent.hpp"
#include "RL/UserAgent.hpp"
#include "RL/Policy.hpp"
#include "RL/StateRepresentation.hpp"
#include "PCBoard.hpp"
#include "Net.hpp"
#include "Connection.hpp"
#include "Util/PCBItemSets.hpp"

Agent::Agent(const std::string &name) : mName(name)
{
    mRewardFn = std::make_unique<rewards::RouteLength>();
    initParameters();
    mStepLock.setGranularity(UserSettings::get().UI.LockSteppingGranularity);
}

Agent::~Agent()
{
    for (uint i = 0; i < 5; ++i)
        py::DECREF_safe(mMethodNamesPy[i]);
    for (auto &I : mParameters)
        delete I.second;
}

bool Agent::run()
{
    if (!mRewardFn || !mPCB)
        throw std::runtime_error("run called without board or reward");
    mRewardFn->setContext(*mPCB);

    if (mConnections.empty())
        return false;
    if (mClearBoardBeforeRun)
        mPCB->wipe();
    else
        eraseManagedConnections();

    mStats.reset();
    resetActionLimit();
    resetActionCount();
    return _run();
}

void Agent::setPCB(const std::shared_ptr<PCBoard> &PCB)
{
    INFO("Agent " << mName << " setPCB = " << (PCB ? PCB->name() : ""));
    mPCB = PCB;
    mConnections.clear();
    mConnectionSet.clear();
    mConnectionLRU = 0;
    if (PCB) {
        if (mStateRep)
            mStateRep->init(*PCB);
        if (mRewardFn)
            mRewardFn->setContext(*PCB);
    }
}
void Agent::setStateRepresentation(StateRepresentation *SR)
{
    mStateRep.reset(SR);
}
void Agent::setPolicy(Policy *P)
{
    mPolicy.reset(P);
}

void Agent::setManagedConnections(const std::set<Connection *> &X)
{
    mConnectionSet = X;
    mConnections.assign(X.begin(), X.end());
    Connection::sort_name(mConnections);
    if (!mPCB)
        return;

    if (mStateRep)
        mStateRep->setFocus(&mConnections);

    Real lmin = 0.0;
    Bbox_2 bbox;
    for (const auto X : mConnections) {
        bbox += X->bbox();
        lmin += X->referenceLen();
    }
    mStats.F32["ReferenceLen"] = lmin;

    mPCB->setActiveAreaBbox(bbox);
}

Connection *Agent::setConnectionLRU(PyObject *py)
{
    if (PyLong_Check(py)) {
        const uint i = PyLong_AsLong(py);
        if (i >= mConnections.size())
            throw std::runtime_error("connection index out of range");
        mConnectionLRU = mConnections[i];
    } else {
        auto X = mConnectionLRU = mPCB->getConnection(py);
        if (!mConnectionSet.empty() && mConnectionSet.find(X) == mConnectionSet.end())
            throw std::runtime_error("requested connection is not in active set");
    }
    return mConnectionLRU;
}

void Agent::eraseManagedConnections()
{
    for (auto X : mConnections)
        if (X->hasTracks())
            mPCB->eraseTracks(*X);
}

Action::Result Agent::evaluateCurrentState() const
{
    Action::Result res;
    res.R = getRewardFn()(mConnections, &res.Router);
    assert(res.Router.isSet());
    res.Success = res.Router.NumDisconnected == 0;
    res.Termination = true;
    return res;
}

void Agent::setPythonInterface(PyObject *py)
{
    Py_Initialize();
    py::GIL_guard GIL;

    py::DECREF_safe(mPython);
    mPython = py;
    Py_INCREF(py);

    if (mMethodNamesPy[0])
        return;
    mMethodNamesPy[PCB_PY_I_METHOD_EVENT] = py::String("event");
    mMethodNamesPy[PCB_PY_I_METHOD_V] = py::String("V");
    mMethodNamesPy[PCB_PY_I_METHOD_Q] = py::String("Q");
    mMethodNamesPy[PCB_PY_I_METHOD_P] = py::String("P");
    mMethodNamesPy[PCB_PY_I_METHOD_P_V] = py::String("P_V");
}

void Agent::setParameter(const std::string &varName, int index, PyObject *py)
{
    const auto name = (index >= 0) ? fmt::format("{}[{}]", varName, index) : varName;
    auto I = mParameters.find(name);
    if (I != mParameters.end())
        I->second->set(py);
    else if (varName == "py_interface")
        setPythonInterface(py);
    else if (varName == "policy")
        setPolicy(Policy::create(Policy::CommonParams::from(py)));
    else if (varName == "reward")
        setRewardFn(RewardFunction::create(py));
    else if (varName.starts_with("ppo") ||
             varName.starts_with("learning") ||
             varName == "batch_size" ||
             varName == "burn_in_batches" ||
             varName == "gae_td_lambda")
        INFO("ignored agent parameter: " << varName);
    else
        throw std::invalid_argument(fmt::format("Unknown parameter: {}", name));
}
void Agent::setParameters(PyObject *py)
{
    if (!PyDict_Check(py))
        throw std::runtime_error("parameter set must be a dictionary");
    PyObject *key;
    PyObject *v;
    Py_ssize_t pos = 0;
    while (PyDict_Next(py, &pos, &key, &v)) {
        const std::string name = py::String_Check(key) ? py::String_AsStdString(key) : "not a string";
        if (PyList_Check(v)) {
            for (uint i = 0; i < PyList_Size(v); ++i)
                setParameter(name, i, PyList_GetItem(v, i));
        } else if (PyTuple_Check(v)) {
            for (uint i = 0; i < PyTuple_Size(v); ++i)
                setParameter(name, i, PyTuple_GetItem(v, i));
        } else {
            setParameter(py::String_AsStdString(key), -1, v);
        }
    }
}
void Agent::initParameters()
{
    mParameters["timeout_us"] = new Parameter("Autoroute Timeout", [this](const Parameter &v){ setTimeout(v.as_i()); });
    mParameters["action_limit"] = new Parameter("Action Limit", [this](const Parameter &v){ setActionLimit(v.i()); });
    mParameters["keep_tracks"] = new Parameter("Keep Existing Tracks", [this](const Parameter &v){ setClearBoardBeforeRun(!v.b()); });
    mParameters["astar_via_cost"] = new Parameter("A-star Via Cost", [](const Parameter &v){ UserSettings::edit().AStarViaCostFactor = v.d(); });

    mParameters["timeout_us"]->setLimits(mTimeoutUSecs, 0, std::numeric_limits<int64_t>::max());
    mParameters["action_limit"]->setLimits(mActionLimit, 0, std::numeric_limits<int32_t>::max());
    mParameters["keep_tracks"]->init(!mClearBoardBeforeRun);
    mParameters["astar_via_cost"]->setLimits(UserSettings::get().AStarViaCostFactor, -std::numeric_limits<double>::max(), std::numeric_limits<double>::max());

    mParameters["timeout_us"]->setVisible(false);
    mParameters["astar_via_cost"]->setVisible(false);
}

std::vector<float> Agent::pycall_floatarray(uint index, PyObject *states)
{
    assert(mPython && index >= 1 && index <= 4);
    auto rv = PyObject_CallMethodObjArgs(mPython, mMethodNamesPy[index], states, 0);
    if (!rv)
        throw std::runtime_error("Python V/P/Q/P_V(states) returned exception");
    Py_DECREF(states);
    return py::NPArray<float>(rv).vector();
}
void Agent::py_event(PyObject *event)
{
    assert(mPython);
    auto rv = PyObject_CallMethodObjArgs(mPython, mMethodNamesPy[PCB_PY_I_METHOD_EVENT], event, 0);
    if (!rv)
        throw std::runtime_error("Python event(...) returned exception");
    Py_DECREF(event);
}

void Agent::setManagedConnectionsToUnrouted()
{
    if (!mPCB)
        throw std::runtime_error("tried to set connections before PCB");
    std::set<Connection *> conns;
    for (auto net : mPCB->getNets())
        for (auto X : net->connections())
            if (!X->isRouted())
                conns.insert(X);
    setManagedConnections(conns);
}

void Agent::setRunArgs(PyObject *py)
{
    py::GIL_guard GIL;

    py::Object args(py);
    if (args.isDict()) {
        PCBItemSets items;
        items.populate(*mPCB.get(), *args);
        items.populateConnectionsFromComs();
        items.populateConnectionsFromPins();
        items.populateConnectionsFromNets();
        setManagedConnections(*items.cons);
    }
    if (mConnections.empty())
        setManagedConnectionsToUnrouted();
    if (!args)
        return;
    if (!args.isDict())
        throw std::invalid_argument("agent run arguments must be a dict");
    if (auto params = args.item("parameters"))
        setParameters(*params);
    if (auto wipe = args.item("wipe"))
        setClearBoardBeforeRun(wipe.asBool());
}

Agent *Agent::create(const std::string &name)
{
    if (name == "user")
        return new UserAgent();
    if (name == "rrr")
        return new RRRAgent();
    if (name == "")
        return new Agent(name);
    return 0;
}
