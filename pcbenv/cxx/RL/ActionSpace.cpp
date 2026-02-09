
#include "Log.hpp"
#include "RL/ActionSpace.hpp"
#include "RL/Action.hpp"

void ActionSpace::clear(bool deleteActions)
{
    if (deleteActions)
        for (auto A : mActions)
            delete A;
    mActions.clear();
    mIllegal.clear();
    mActionMap.clear();
}

Action *ActionSpace::getAction(PyObject *py) const
{
    if (PyLong_Check(py))
        return getAction(PyLong_AsLong(py));
    if (py::String_Check(py))
        return getAction(py::String_AsStdString(py));
    return 0;
}

void ActionSpace::addAction(Action *A)
{
    if (A->index() == std::numeric_limits<Action::Index>::max())
        A->setIndex(mActions.size());
    if (A->index() >= mActions.size())
        mActions.resize(A->index() + 1, 0);
    if (mActions[A->index()] != 0 && mActions[A->index()] != A)
        WARN("action with index " << A->index() << " already added, overriding");
    if (getAction(A->name()))
        WARN("action with name " << A->name() << " already added, overriding");
    mActions[A->index()] = A;
    mActionMap[A->name()] = A;
    DEBUG("action[" << A->index() << "] = " << A->name());
}

void ActionSpace::setLegal(Action *A, bool b)
{
    assert(mActions.at(A->index()) == A);
    const auto I = mIllegal.find(A->index());
    assert(A->legal() == (I == mIllegal.end()));
    if (A->legal() == b)
        return;
    A->setLegal(b);
    if (b)
        mIllegal.erase(I);
    else
        mIllegal.insert(A->index());
}

void ActionSet::insert(Action::Index a)
{
    auto I = mActions.begin();
    for (I = mActions.begin(); I != mActions.end() && *I <= a; ++I)
        if (*I == a)
            return;
    mActions.insert(I, a);
}
void ActionSet::remove(Action::Index a)
{
    auto I = mActions.begin();
    for (I = mActions.begin(); I != mActions.end() && *I <= a; ++I) {
        if (*I == a) {
            mActions.erase(I);
            return;
        }
    }
}

void ActionSet::init(ActionSpace &AS, Action::Index first, Action::Index last)
{
    mAS = &AS;
    mActions.reserve(last - first + 1);
    while (first <= last)
        mActions.push_back(first++);
}
void ActionSet::init(ActionSpace &AS, const std::set<Action::Index> *A)
{
    mAS = &AS;
    if (A) {
        mActions.resize(A->size());
        uint i = 0;
        for (auto a : *A)
            mActions[i++] = a;
    } else {
        mActions.resize(AS.size());
        for (uint a = 0; a < AS.size(); ++a)
            mActions[a] = a;
    }
}

PyObject *ActionSpace::getPy() const
{
    auto py = py::Object(PyDict_New());
    for (const auto A : mActions)
        py.setDictItem(A->index(), A->getPy());
    return *py;
}
