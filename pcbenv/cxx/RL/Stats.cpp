
#include "RL/Stats.hpp"
#include "RL/Agent.hpp"

ResultRecord::ResultRecord(const Agent &A)
{
    Iteration = A.getIteration();
    TimeMicroseconds = A.usecsSinceStart();
    TimeStep = A.numActions();
}
ResultRecord::ResultRecord(const Agent &A, const Action::Result &res) : ResultRecord(A)
{
    Success = res.Success;
    RewardSum = res.R;
    Router = res.Router;
}

std::string ResultRecord::str() const
{
    std::stringstream ss;
    assert(Success == !Router.NumDisconnected);
    ss << (Success ? "SUCCESS" : "FAILURE");
    ss << " Return=" << RewardSum;
    ss << " Success=" << Success;
    ss << " Length=" << Router.TrackLen;
    ss << " Vias=" << Router.NumVias;
    ss << " Unrouted=" << Router.NumDisconnected;
    ss << " I=" << Iteration << " T=" << TimeStep << "/" << (TimeMicroseconds * 1e-6) << 's';
    return ss.str();
}

PyObject *ResultRecord::getPy() const
{
    auto py = PyDict_New();
    py::Dict_StealItemString(py, "Success", PyBool_FromLong(Success));
    py::Dict_StealItemString(py, "RewardSum", PyFloat_FromDouble(RewardSum));
    py::Dict_StealItemString(py, "Iteration", PyLong_FromLong(Iteration));
    py::Dict_StealItemString(py, "TimeUSec", PyLong_FromLong(TimeMicroseconds));
    py::Dict_StealItemString(py, "TimeStep", PyLong_FromLong(TimeStep));
    py::Dict_StealItemString(py, "NumVias", PyLong_FromLong(Router.NumVias));
    py::Dict_StealItemString(py, "TrackLen", PyLong_FromLong(Router.TrackLen));
    py::Dict_StealItemString(py, "NumUnrouted", PyLong_FromLong(Router.NumDisconnected));
    return py;
}

PyObject *ResultCollection::getPy() const
{
    auto res = py::Object(PyDict_New());
    res.setItem("TimeLine", py::Object::new_ListFrom(TimeLine));
    res.setItem("WorstSuccess", WorstSuccess.getPy());
    res.setItem("Worst", Worst.getPy());
    for (const auto &I : F32)
        res.setItem(I.first.c_str(), *py::Object(I.second));
    for (const auto &I : I64)
        res.setItem(I.first.c_str(), *py::Object(I.second));
    return *res;
}
