
#include "RL/Action.hpp"

Action::Result Action::undoAs(Agent &A)
{
    if (!mUndo)
        throw std::runtime_error("This action does not support undo()");
    return mUndo->performAs(A, 0);
}

bool Action::Result::operator==(const Action::Result &that) const
{
    if (Success != that.Success || R != that.R)
        return false;
    if (Router.isSet() != that.Router.isSet())
        throw std::runtime_error("results comparison is undefined");
    return Router.isSet() ? (Router == that.Router) : true;
}

std::string Action::Result::str() const
{
    std::stringstream ss;
    ss << (Success ? "SUCCESS" : "FAILURE");
    ss << "(R: " << R << "|T: " << Termination;
    if (Router.isSet())
        ss << "|Len: " << Router.TrackLen << "|Vias: " << Router.NumVias << "|Left: " << Router.NumDisconnected;
    ss << ")";
    return ss.str();
}

PyObject *Action::getPy() const
{
    return py::String(name());
}
