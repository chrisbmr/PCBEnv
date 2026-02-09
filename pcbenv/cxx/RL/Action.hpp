
#ifndef GYM_PCB_RL_ACTION_H
#define GYM_PCB_RL_ACTION_H

#include "Defs.hpp"
#include "RL/Reward.hpp"
#include <string>

class Agent;
class NavGrid;

/**
 * Base class for environment actions that can be called by the user via step() or used internally.
 * Actions operating on a single connection should use the Agent::setConnectionLRU().
 * The effect of an action may be cached given a state identified by a CacheRef (this must be implemented by the subclass).
 * Each action may have an associated undo action (this must also be managed by the subclass).
 * Action index and legal state are managed by action space implementations.
 */
class Action
{
public:
    using Index = uint16_t;
    constexpr static const Index IndexNone = std::numeric_limits<Index>::max();

    using CacheRef = uint32_t;

    /**
     * An action's result, including a reward, routing details, action success, and a termination indicator.
     */
    struct Result {
        Result() { }
        Result(Reward r, bool ok, bool t = false) : R(r), Success(ok), Termination(t) { }
        bool operator<(const Result&) const;
        bool operator>(const Result &that) const { return that < *this; }
        bool operator==(const Result&) const;
        std::string str() const;
        Reward R;
        RouterResult Router;
        bool Success{true}; // Don't change the defaults!
        bool Termination{false};
    };
public:
    Action(const std::string &name, uint index = std::numeric_limits<Index>::max()) : mName(name), mIndex(index) { }
    virtual ~Action() { }

    const std::string& name() const { return mName; }
    Index index() const { return mIndex; }
    bool legal() const { return mLegal; }

    virtual Result performAs(Agent& context, PyObject *arg = 0) { return Result(); }

    virtual void clearCache(CacheRef = 0) { }
    virtual Result performAs(Agent& context, PyObject *arg, CacheRef) { return performAs(context); }

    virtual Result undoAs(Agent&);
    Action *getUndo() const { return mUndo; }

    void setUndo(Action *A) { mUndo = A; }
    void setIndex(Index a) { mIndex = a; }
    void setLegal(bool b) { mLegal = b; }

    void setActionCountIncrement(uint n) { mActionCountIncrement = n; }

    virtual PyObject *getPy() const;
protected:
    Action *mUndo{0};
    uint mActionCountIncrement{1};
private:
    const std::string mName;
    Index mIndex{std::numeric_limits<Index>::max()};
    bool mLegal;
};
inline bool Action::Result::operator<(const Action::Result &that) const
{
    return (Success < that.Success) || (Success == that.Success && R < that.R);
}

#endif // GYM_PCB_RL_ACTION_H
