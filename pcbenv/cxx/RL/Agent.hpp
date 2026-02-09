#ifndef GYM_PCB_RL_AGENT_H
#define GYM_PCB_RL_AGENT_H

#include "Defs.hpp"
#include "Py.hpp"
#include "Parameter.hpp"
#include "Signals.hpp"
#include "RL/Action.hpp"
#include "RL/Stats.hpp"
#include "UI/LockStep.hpp"
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

class ActionSpace;
class Policy;
class StateRepresentation;
class PCBoard;
class Connection;

/**
 * Python method indices.
 * If env.set_interface(object) was called, we can call the following object methods:
 * - event(object): report something to the Python object
 * - V(states): get the value function for the states
 * - Q(states): get the action-value function for the states
 * - P(states): get the policy P(a|s) for P({s=T(s',a)}) for s in states
 * - P_V(states): get the policy and the value function for the states
 */
constexpr const uint PCB_PY_I_METHOD_EVENT = 0;
constexpr const uint PCB_PY_I_METHOD_V = 1;
constexpr const uint PCB_PY_I_METHOD_Q = 2;
constexpr const uint PCB_PY_I_METHOD_P = 3;
constexpr const uint PCB_PY_I_METHOD_P_V = 4;

/**
 * This is the base class for the C++ representation of a reinforcement learning agent combined with an action space and a reward function.
 */
class Agent
{
public:
    static Agent *create(const std::string &type);
public:
    Agent(const std::string &name);
    virtual ~Agent();
    const std::string& name() const { return mName; }
    virtual void setStateRepresentationParams(PyObject *) { }
    void setParameters(PyObject *);
    virtual void setParameter(const std::string &varName, int index, PyObject *);
    const std::map<std::string, Parameter *>& getParameters() const { return mParameters; }
    void setPythonInterface(PyObject *);
    void setTrainingMode(bool on);

    virtual void setPCB(const std::shared_ptr<PCBoard>&);
    virtual void setManagedConnections(const std::set<Connection *>&);
    void setManagedConnectionsToUnrouted();
    void setActionSpace(const ActionSpace&);
    void setPolicy(Policy *); //!< ownership is transferred to this class
    void setRewardFn(RewardFunction *); //!< ownership is transferred to this class
    void setStateRepresentation(StateRepresentation *); //!< ownership is transferred to this class

    virtual bool _run() { return false; }
    bool run(); //!< C++ implementation of agent behaviour
    void setRunArgs(PyObject *);
    void setTimeout(uint64_t usec); //!< timeout for run()
    void setTimeoutExpired();
    void setActionLimit(int64_t count);
    void setClearBoardBeforeRun(bool);
    float getProgress() const { return mProgress; }

    virtual PyObject *step(PyObject *) { return 0; }
    virtual PyObject *get_state(PyObject *) { return 0; }
    virtual PyObject *reset() { return 0; }

    void eraseManagedConnections();

    PCBoard *getPCB() const { return mPCB.get(); }
    RewardFunction& getRewardFn() const { return *mRewardFn.get(); }
    Policy *getPolicy() const { return mPolicy.get(); }
    StateRepresentation *getSR() const { return mStateRep.get(); }
    uint32_t getIteration() const { return mIteration; }
    uint64_t numActions() const { return mNumActions; }
    uint64_t usecsSinceStart() const;
    const std::vector<Connection *>& getConnections() const { return mConnections; }
    const std::set<Connection *>& getConnectionSet() const { return mConnectionSet; }
    Connection *setConnectionLRU(PyObject *);
    Connection *getConnectionLRU() const { return mConnectionLRU; }
    Action::Result evaluateCurrentState() const;

    void startTimer();
    bool hasTimerExpired() const;
    void resetActionCount();
    void resetActionLimit();
    void countActions(int);
    bool actionLimitExceeded() const;

    bool havePythonInterface() const { return !!mPython; }

    /**
     * Policy and value functions from Python (e.g. neural networks).
     * The format of the return vector is up to the user.
     * Arguments are DECREF'd.
     */
    std::vector<float> py_V(PyObject *states) { return pycall_floatarray(PCB_PY_I_METHOD_V, states); }
    std::vector<float> py_Q(PyObject *states) { return pycall_floatarray(PCB_PY_I_METHOD_Q, states); }
    std::vector<float> py_P(PyObject *states) { return pycall_floatarray(PCB_PY_I_METHOD_P, states); }
    std::vector<float> py_P_V(PyObject *states) { return pycall_floatarray(PCB_PY_I_METHOD_P_V, states); }

    void py_event(PyObject *event);

    StepLock& getStepLock() { return mStepLock; } //!< for the UI
protected:
    std::shared_ptr<PCBoard> mPCB;
    std::vector<Connection *> mConnections; //!< connections managed by this agent
    std::set<Connection *> mConnectionSet;
    Connection *mConnectionLRU{0};
    const ActionSpace *mActionSpace{0};
    std::unique_ptr<Policy> mPolicy;
    std::unique_ptr<StateRepresentation> mStateRep;
    std::unique_ptr<RewardFunction> mRewardFn;
    ResultCollection mStats;
    uint32_t mIteration{0};
    float mProgress{-1.0f}; //!< set this from 0 to 1 when running for UI progress bar
    bool mClearBoardBeforeRun{false};
    SignalContext mSignals;
    std::map<std::string, Parameter *> mParameters;

    void initParameters();

    PyObject *mPython{0};
    PyObject *mMethodNamesPy[5]{0,0,0,0,0};
private:
    int64_t mActionLimit{std::numeric_limits<int64_t>::max()};
    int64_t mNumActions{0};
    int64_t mActionsLeft{std::numeric_limits<int64_t>::max()};
    uint64_t mTimeoutUSecs{0};
    std::chrono::time_point<std::chrono::system_clock> mTimerStartPoint;
    std::chrono::time_point<std::chrono::system_clock> mTimeoutPoint;
    bool mTraining{false};
    mutable StepLock mStepLock; //!< for UI
    const std::string mName;

    std::vector<float> pycall_floatarray(uint index, PyObject *);
};

inline void Agent::setRewardFn(RewardFunction *RF)
{
    if (RF && mPCB)
        RF->setContext(*mPCB);
    mRewardFn.reset(RF);
}

inline void Agent::setActionSpace(const ActionSpace &A)
{
    mActionSpace = &A;
}

inline void Agent::setClearBoardBeforeRun(bool b)
{
    mClearBoardBeforeRun = b;
}

inline void Agent::setTimeout(uint64_t usec)
{
    mTimeoutUSecs = usec;
}
inline uint64_t Agent::usecsSinceStart() const
{
    const auto now = std::chrono::system_clock::now();
    return
        std::chrono::duration_cast<std::chrono::microseconds>(now - mTimerStartPoint).count();
}
inline void Agent::setTimeoutExpired()
{
    mTimeoutPoint = std::chrono::system_clock::now();
}
inline void Agent::startTimer()
{
    mTimerStartPoint = std::chrono::system_clock::now();
    if (mTimeoutUSecs)
        mTimeoutPoint = mTimerStartPoint + std::chrono::microseconds(mTimeoutUSecs);
    else
        mTimeoutPoint = std::chrono::system_clock::time_point::max();
}
inline bool Agent::hasTimerExpired() const
{
    return std::chrono::system_clock::now() >= mTimeoutPoint;
}

inline void Agent::setActionLimit(int64_t n)
{
    mActionLimit = (n > 0) ? n : std::numeric_limits<int64_t>::max();
}
inline void Agent::resetActionCount()
{
    mNumActions = 0;
}
inline void Agent::resetActionLimit()
{
    mActionsLeft = mActionLimit;
}
inline void Agent::countActions(int n)
{
    mNumActions += n;
    mActionsLeft -= n;
    mStepLock.wait(10);
}
inline bool Agent::actionLimitExceeded() const
{
    return mActionsLeft <= 0;
}

inline void Agent::setTrainingMode(bool b)
{
    mTraining = b;
}

#endif // GYM_PCB_RL_AGENT_H
