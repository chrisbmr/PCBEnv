
#ifndef GYM_PCB_RL_ACTIONSPACE_H
#define GYM_PCB_RL_ACTIONSPACE_H

#include "Py.hpp"
#include "Action.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

class ActionSpace
{
public:
    virtual ~ActionSpace() { clear(mOwnsActions); }
    virtual void clear(bool deleteActions);
    uint size() const { return mActions.size(); }
    const std::vector<Action *>& actions() const { return mActions; }
    Action *getAction(Action::Index a) const { return mActions.at(a); }
    Action *getAction(const std::string &name) const;
    Action *getAction(PyObject *) const;
    void addAction(Action *);
    void setOwnsActions(bool own) { mOwnsActions = own; }
    bool ownsActions() const { return mOwnsActions; }
    const std::set<Action::Index>& getIllegal() const { return mIllegal; }
    void setLegal(Action *, bool);
    PyObject *getPy() const;
protected:
    std::vector<Action *> mActions;
private:
    std::set<Action::Index> mIllegal;
    std::map<std::string, Action *> mActionMap;
    bool mOwnsActions{false};
};
inline Action *ActionSpace::getAction(const std::string &name) const
{
    auto I = mActionMap.find(name);
    if (I != mActionMap.end())
        return I->second;
    return 0;
}

class ActionSet
{
public:
    void init(ActionSpace&, Action::Index first, Action::Index last);
    void init(ActionSpace&, const std::set<Action::Index> * = 0);
    const std::vector<Action::Index> actions() const { return mActions; }
    uint size() const { return mActions.size(); }
    void clear() { mActions.clear(); }
    void insert(Action::Index);
    void remove(Action::Index);
    ActionSpace *space() const { return mAS; }
private:
    ActionSpace *mAS{0};
    std::vector<Action::Index> mActions;
};

#endif // GYM_PCB_RL_ACTIONSPACE_H
