
#include "Pin.hpp"
#include "Connection.hpp"
#include "Net.hpp"
#include "Clone.hpp"

Pin::Pin(Component *parent, const std::string &name) : Object(parent, name)
{
}

Pin::~Pin()
{
    // Do not delete pins that are in any nets!
    assert(!mNet);
    setCompound(0);
}

void Pin::setNet(Net *net)
{
    assert(mNet || mConnections.empty());
    if (mNet == net)
        return;
    if (mNet && net)
        throw std::runtime_error("pin must be removed from the old net before setting a new one");
    mNet = net;
    mConnections.clear();
}

bool Pin::removeConnection(Connection *X, bool retainNet)
{
    const bool rv = mConnections.erase(X) > 0;
    if (rv && mConnections.empty() && mNet && !retainNet)
        mNet->remove(*this);
    return rv;
}

uint Pin::getStartLayerFor(const Connection &X) const
{
    if (minLayer() < 0)
        throw std::runtime_error("pin does not have any valid layers");
    for (int z = minLayer(); z <= maxLayer(); ++z)
        if (X.canRouteOnLayer(z))
            return z;
    return minLayer();
}

void Pin::setCompound(Pin *P)
{
    if (!P && mCompound) {
        mCompound->erase(std::find(mCompound->begin(), mCompound->end(), this));
        if (mCompound->empty())
            delete mCompound;
        mCompound = 0;
    } else if (P) {
        if (mCompound)
            throw std::runtime_error("pin is already part of a compound");
        if (!P->mCompound)
            P->mCompound = new PinCompound(1, P);
        mCompound = P->mCompound;
        mCompound->push_back(this);
    }
}

std::string Pin::str() const
{
    std::stringstream ss;
    ss << "(#" << id() << ' ' << getParent()->name();
    if (hasNet())
        ss << " <=> " << net()->id();
    if (mCompound)
        for (auto P : *mCompound)
            ss << " @" << P->name();
    ss << ')';
    return ss.str();
}

Object *Pin::clone(CloneEnv &env) const
{
    auto P = new Pin(env.C[getParent()], name());
    env.P[this] = P;
    if (mCompound) {
        auto K = env.PK.find(mCompound);
        P->mCompound = (K != env.PK.end()) ? K->second : new PinCompound(*mCompound);
        if (K == env.PK.end())
            env.PK[mCompound] = P->mCompound;
        auto I = std::find(P->mCompound->begin(), P->mCompound->end(), this);
        assert(I != P->mCompound->end());
        *I = P;
    }
    P->copyFrom(env, *this);
    return P;
}

PyObject *Pin::getPy(uint depth) const
{
    auto py = Object::getPy(depth);
    if (!py)
        return 0;

    std::vector<Pin *> others;
    for (auto X : mConnections)
        if (X->otherPin(this))
            others.push_back(X->otherPin(this));
    auto links = PyList_New(others.size());
    uint i = 0;
    for (auto P : others)
        PyList_SetItem(links, i++, py::String(P->getFullName()));

    py::Dict_StealItemString(py, "connects_to", links);
    py::Dict_StealItemString(py, "net", hasNet() ? py::String(net()->name()) : py::None());
    if (mCompound) {
        auto K = PyList_New(mCompound->size());
        i = 0;
        for (const auto *P : *mCompound)
            PyList_SetItem(K, i++, py::String(P->name()));
        py::Dict_StealItemString(py, "compound", K);
    }
    return py;
}
