
#include "Log.hpp"
#include "Component.hpp"
#include "Pin.hpp"
#include "Net.hpp"
#include "Clone.hpp"
#include <sstream>

Component::Component(const std::string &name) : Object(0, sanitizeName(name))
{
}

Component::~Component()
{
    // NOTE: Pins are deleted by Object dtor.
}

void Component::setLocation(const Point_2 &refPoint, int layer)
{
    mRefPoint = refPoint;
    setLayer(layer);
    mColorLine = Palette::FootprintLine[layer ? 1 : 0];
    mColorFill = Palette::FootprintFill[layer ? 1 : 0];
}

Pin *Component::addPin(const std::string &name)
{
    if (name.empty())
        return addPin(std::string("§") + std::to_string(numPins()));
    if (getPin(name))
        throw std::runtime_error(fmt::format("pin {} already exists on component", name));
    auto pin = new Pin(this, name);
    if (!pin)
        return 0;
    pin->setLayer(getSingleLayer());
    addChild(*pin);
    mPinMap[name] = pin;
    return pin;
}

Pin *Component::removeAndDisconnectPin(Pin &P)
{
    removeChild(P);
    mPinMap.erase(P.name());
    if (P.hasNet())
        P.net()->remove(P);
    return &P;
}

void Component::gatherConnections(std::set<Connection *> &res) const
{
    for (const auto *I : getPins())
        if (auto P = I->as<Pin>())
            res.insert(P->connections().begin(), P->connections().end());
}

void Component::addOccludedObject(Object &occ)
{
    WARN("Component " << name() << " occludes " << occ.name());

    setCanRouteInside(true);
}

uint Component::connectivity(std::vector<std::pair<Pin *, Pin *>> &pins,
                             const Component &A,
                             const Component &B)
{
    const auto &C1 = A.numPins() < B.numPins() ? A : B;
    const auto &C2 = A.numPins() < B.numPins() ? B : A;
    uint n = 0;
    for (auto I : C1.getPins()) {
        auto P1 = I->as<Pin>();
        if (!P1 ||
            !P1->hasNet())
            continue;
        bool sane = false;
        for (auto P2 : P1->net()->getPins()) {
            if (P2->getParent() == &C2) {
                n += 1;
                pins.push_back(std::make_pair(P1, P2));
            } else if (P2->getParent() == &C1) {
                sane = true; // at least one pin of this net must be part of C1
            }
        }
        if (!sane)
            throw std::runtime_error("inconsistency in pin/net membership detected");
    }
    return n;
}

Object *Component::clone(CloneEnv &env) const
{
    auto C = new Component(name());
    env.C[this] = C;
    C->mRefPoint = mRefPoint;
    C->mColorLine = mColorLine;
    C->mColorFill = mColorFill;
    C->mPinMap = mPinMap;
    for (auto P : getChildren())
        C->mChildren.push_back(P->clone(env));
    C->copyFrom(env, *this);
    return C;
}

std::string Component::str() const
{
    std::stringstream ss;
    ss << '[' << name() << '/' << id() << " @ ";
    if (isPlaced())
        ss << getCenter().x() << ',' << getCenter().y() << ',' << mLayerRange[0];
    else
        ss << "N/A";
    ss << "]{";
    for (auto pin : getChildren())
        ss << '\n' << pin->str() << " @ " << pin->getCenter();
    ss << '}';
    ss << getShape()->str();
    return ss.str();
}

PyObject *Component::getPy(uint depth) const
{
    auto py = Object::getPy(depth);
    if (!py)
        return 0;
    py::Dict_StealItemString(py, "can_route", PyBool_FromLong(mCanRouteInside));
    py::Dict_StealItemString(py, "via_ok", PyBool_FromLong(mCanPlaceViasInside));
    PyObject *pins = depth ? PyDict_New() : PyLong_FromLong(numPins());
    if (depth)
        for (auto P : getPins())
            py::Dict_StealItemString(pins, P->name().c_str(), P->getPy(depth - 1));
    py::Dict_StealItemString(py, "pins", pins);
    return py;
}

std::string Component::sanitizeName(const std::string &s0)
{
    bool hasHyphens = std::count(s0.begin(), s0.end(), '-') > 0;
    if (!hasHyphens)
        return s0;
    std::string s = s0;
    std::replace(s.begin(), s.end(), '-', '_');
    WARN("Component \"" << s0 << "\" renamed to \"" << s << '"');
    return s;
}
