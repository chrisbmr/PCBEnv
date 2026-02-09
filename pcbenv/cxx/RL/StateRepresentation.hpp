
#ifndef GYM_PCB_RL_STATEREPRESENTATION_H
#define GYM_PCB_RL_STATEREPRESENTATION_H

#include "Py.hpp"

class Action;
class PCBoard;
class Connection;

/**
 * The StateRepresentation object abstracts the state representation and returns it in a PyObject.
 * As such, using it makes sense only for use with Python policies.
 *
 * The subclass create(PyObject *) functions check if the PyObject matches and return a new instace if it does, or 0 otherwise (or throw an exception if the name matches but the parameters don't).
 */
class StateRepresentation
{
public:
    /// Create a new intance of a subclas as specified by the PyObject.
    static StateRepresentation *create(PyObject *);
public:
    virtual PyObject *getPy(PyObject *args) { return py::None(); }

    virtual void init(PCBoard &PCB) { mPCB = &PCB; }
    virtual ~StateRepresentation() { }
    virtual void reset() { }
    virtual const char *name() const { return ""; }

    /**
     * Use setFocus() if you want to create an asymmetry in the state by highlighting a specific connection.
     * This can also be an argument to getPy().
     */
    void setFocus(const Connection *);

    /**
     * Define the set of connections covered by the state representation.
     */
    virtual void setFocus(const std::vector<Connection *> *) { }

    virtual void updateView(const Bbox_2 * = 0) { }

    /**
     * Prevent updates of the part of the board captured by the representation, such as the bounding box for images.
     */
    void setLockedView(bool v) { mLockedView = v; }

    /**
     * logAction() can be used for state representations like an action sequence as used e.g. by simple combinatorial optimization, or for efficiency.
     */
    virtual void logAction(const Action *) { }
protected:
    PCBoard *mPCB{0};
    const Connection *mFocus{0};
    bool mLockedView{false};
};

inline void StateRepresentation::setFocus(const Connection *X)
{
    mFocus = X;
}

#endif // GYM_PCB_RL_STATEREPRESENTATION_H
