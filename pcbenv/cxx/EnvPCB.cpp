#include "EnvPCB.hpp"
#include "Py.hpp"
#include "Log.hpp"
#include "PCBoard.hpp"
#include "UserSettings.hpp"
#include "Loaders/Factory.hpp"
#include "RL/Agent.hpp"
#include "UI/Application.hpp"
#include "Util/PCBItemSets.hpp"

EnvPCB::EnvPCB()
{
    mAgent.reset(Agent::create("user"));
}

EnvPCB::~EnvPCB()
{
}

PyObject *EnvPCB::reset()
{
    return mAgent->reset();
}

PyObject *EnvPCB::step(PyObject *action)
{
    return mAgent->step(action);
}

void EnvPCB::render(PyObject *_command)
{
    std::string_view command;
    {
    py::GIL_guard GIL;

    if (py::String_Check(_command))
        command = py::String_AsStringView(_command);
    }
    
    if (mUI) {
        if (command == "show")
            mUI->show();
        else if (command == "hide")
            mUI->hide();
        else if (command == "wait")
            mUI->wait();
    } else if (command == "human" || command == "qt") {
        mUI.reset(IUIApplication::create());
        if (!mUI)
            return;
        auto hasStarted = mUI->runAsync();
        if (!hasStarted.get())
            return;
        if (mBoard)
            mUI->setPCB(mBoard);
    }
}

void EnvPCB::seed(PyObject *)
{
}

void EnvPCB::close()
{
    if (mUI)
        mUI->quit(0);
}

PyObject *EnvPCB::set_task(PyObject *py)
{
    py::GIL_guard GIL;

    if (!py || !PyDict_Check(py))
        return py::ValueError("Task must be a dict.");
    py::Object args(py);

    auto pdes = args.item("pdes");
    auto json = args.item("json");
    if (!pdes && !json)
        return py::ValueError("set_task: must specify a board");
    if (json && !json.isString())
        return py::ValueError("set_task: json must be a string.");
    if (pdes && !pdes.isString())
        return py::ValueError("set_task: pdes must be a string.");

    PCBoard *board;
    try {
        PCBFactory F;
        if (auto resolution_nm = args.item("resolution_nm")) {
            if (resolution_nm.isNumber())
                F.setUnitLength_nm(Nanometers(resolution_nm.toDouble()));
            else if (resolution_nm.asString() == "design")
                F.setForceDesignRes(true);
            else if (resolution_nm.asString() != "auto")
                throw std::invalid_argument("resolution_nm must be a number, \"design\", or \"auto\"");
        }
        if (auto no_polygons = args.item("no_polygons"))
            F.setLoadPolygonsAsBoxes(no_polygons.asBool());
        if (auto autocreate = args.item("autocreate_connections"))
            F.setAutocreateConnections(autocreate.asBool());
        if (auto min_margin = args.item("min_margin"))
            F.setLayoutAreaMinMargin(min_margin.toDouble());
        if (auto max_margin = args.item("max_margin"))
            F.setLayoutAreaMaxMargin(max_margin.toDouble());
        if (auto rectify_degrees = args.item("rectify_degrees"))
            F.setRectifySegmentsDegrees(rectify_degrees.toDouble());
        if (auto net_overrides = args.item("net_overrides"))
            F.setNetOverrides(*net_overrides);
        if (auto net_topologies = args.item("net_topologies"))
            F.setTopologies(*net_topologies);
        if (auto lock_routed = args.item("lock_routed"))
            F.setLockRoutedConnections(lock_routed.asBool());
        if (auto fixed_track_params = args.item("fixed_track_params"))
            F.setFixedTrackParams(fixed_track_params.asBool());
        if (auto load_tracks = args.item("load_tracks"))
            F.setLoadTracks(*load_tracks);
        board = json ? F.create(json.asString()) : F.loadAndCreate(pdes.asString());
    } catch (std::exception &e) {
        return py::Exception(e);
    }
    if (!board)
        return py::Exception(PyExc_Exception, "Task could not be loaded");

    if (auto min_clearance = args.item("min_clearance")) {
        if (!min_clearance.isLong())
            return py::ValueError("set_task: min_clearance must be an integer (micrometers)");
        const auto c = min_clearance.asLong();
        board->setMinClearancePins(Micrometers(c));
        board->setMinClearanceNets(Micrometers(c));
    }
    if (auto min_track_width = args.item("min_trace_width")) {
        int64_t w = min_track_width.isLong() ? min_track_width.asLong() : -1;
        if (w < 0)
            return py::ValueError("set_task: min_track_width must be a positive integer (micrometers)");
        board->setMinTraceWidth(Micrometers(w));
    }
    if (auto min_via_diam = args.item("min_via_diam")) {
        int64_t d = min_via_diam.isLong() ? min_via_diam.asLong() : -1;
        if (d < 0)
            return py::ValueError("set_task: min_via_diam must be a positive integer (micrometres)");
        board->setMinViaDiameter(Micrometers(d));
    }
    if (auto max_layers = args.item("max_layers")) {
        const auto NL = max_layers.isLong() ? max_layers.asLong() : -1;
        if (NL < 1)
            return py::ValueError("set_task: max_layers must be an integer >= 1");
        if (NL < board->getNumLayers())
            board->setNumLayers(NL);
    }
    {
        PCBItemSets sets;
        try {
            sets.populate(*board, py);
        } catch (std::exception &e) {
            return py::Exception(e);
        }
        if (!sets._cons.empty())
            board->pruneConnections(sets._cons);
        if (!sets._nets.empty())
            board->pruneNets(sets._nets);
        if (!sets._pins.empty())
            board->prunePins(sets._pins);
        if (!sets._coms.empty())
            board->pruneComponents(sets._coms);
    }
    board->getNavGrid().build();
    board->forceConnectionsToGrid();

    board->getNavGrid().initSpacingsForAnyRoutedTrack();

    mBoard.reset(board);
    if (mUI)
        mUI->setPCB(mBoard);

    try {
        if (mUI)
            mUI->startExclusiveTask("Changing agent board");
        if (auto srep = args.item("state_representation"))
            mAgent->setStateRepresentationParams(*srep);
        mAgent->setPCB(mBoard);
        if (mUI)
            mUI->endExclusiveTask();
    } catch (const std::exception &e) {
        return py::Exception(e);
    }

    return PyBool_FromLong(1);
}

PyObject *EnvPCB::set_agent(PyObject *args)
{
    py::GIL_guard GIL;

    if (!PyTuple_Check(args) || PyTuple_Size(args) != 2 || !py::String_Check(PyTuple_GetItem(args, 0)) || !PyDict_Check(PyTuple_GetItem(args, 1)))
        return py::ValueError("set_agent expected a tuple(name: string, parameters: dict)");
    const auto name = py::String_AsStdString(PyTuple_GetItem(args, 0));
    if (!mAgent || mAgent->name() != name)
        mAgent.reset(Agent::create(name));
    if (!mAgent)
        return py::ValueError(fmt::format("could not create agent {}", name).c_str());
    try {
        mAgent->setParameters(PyTuple_GetItem(args, 1));
    } catch (const std::invalid_argument &e) {
        return py::ValueError(e.what());
    } catch (const std::exception &e) {
        return py::Exception(e);
    }
    if (mUI)
        mUI->setAutonomousAgent(mAgent, mBoard);
    else
        mAgent->setPCB(mBoard);
    return PyBool_FromLong(true);
}

PyObject *EnvPCB::run_agent(PyObject *args)
{
    bool rv = false;
    assert(mAgent);
    try {
        if (mUI)
            mUI->startExclusiveTask();
        if (!mAgent)
            throw std::runtime_error("run_agent: no active agent");
        if (args)
            mAgent->setRunArgs(args);
        rv = mAgent->run();
    } catch (std::runtime_error &e) {
        ERROR(e.what());
        if (mUI)
            mUI->endExclusiveTask(true);
        return py::Exception(e);
    }
    if (mUI)
        mUI->endExclusiveTask(true);
    return PyBool_FromLong(rv);
}

PyObject *EnvPCB::get_state(PyObject *spec)
{
    return mAgent->get_state(spec);
}

PyObject *EnvPCB::__str__() const
{
    return py::String("Gym: Printed Circuit Board");
}

Env *create_env(PyObject *spec)
{
    if (!PyDict_Check(spec))
        return 0;
    auto name = PyDict_GetItemString(spec, "name");
    auto PATH = PyDict_GetItemString(spec, "PATH");
    auto conf = PyDict_GetItemString(spec, "settings_json");
    if (!name ||
        !PATH ||
        !py::String_Check(name) ||
        !PyList_Check(PATH) ||
        PyList_Size(PATH) == 0)
        return 0;
    for (uint i = 0; i < PyList_Size(PATH); ++i) {
        auto I = PyList_GetItem(PATH, i);
        if (!py::String_Check(I))
            return 0;
        UserSettings::edit().AddPath(py::String_AsStringView(I));
    }
    if (!UserSettings::get().Paths.JSON.empty())
        UserSettings::LoadFile(UserSettings::get().Paths.JSON + "settings.json");
    if (conf && py::String_Check(conf))
        UserSettings::LoadJSON(py::String_AsStdString(conf));
    return new EnvPCB();
}

#ifndef GYM_PCB_ENABLE_UI

IUIApplication *IUIApplication::create()
{
    return 0;
}

#endif // !GYM_PCB_ENABLE_UI
