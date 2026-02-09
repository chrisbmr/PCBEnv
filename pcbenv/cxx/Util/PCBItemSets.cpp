
#include "Util/PCBItemSets.hpp"
#include "PCBoard.hpp"
#include "Component.hpp"
#include "Connection.hpp"
#include "Net.hpp"

namespace util
{

template<typename T> void _populate(std::set<T *> &set, PyObject *py, const std::function<T*(PyObject *)> &fetch)
{
    set.clear();
    if (!py || py == Py_None)
        return;
    auto I = PyObject_GetIter(py);
    if (!I)
        throw std::invalid_argument("object set: value is not iterable");
    while (auto v = PyIter_Next(I)) {
        T *A = fetch(v);
        Py_DECREF(v);
        if (!A)
            break;
        set.insert(A);
    }
    Py_DECREF(I);
}

void populate(std::set<Connection *> &set, PCBoard &PCB, PyObject *py)
{
    _populate<Connection>(set, py, std::bind(&PCBoard::getConnection, &PCB, std::placeholders::_1));
}

void populate(std::set<Net *> &set, PCBoard &PCB, PyObject *py)
{
    _populate<Net>(set, py, [&PCB](PyObject *arg){return PCB.getNet(arg);});
}

void populate(std::set<Pin *> &set, PCBoard &PCB, PyObject *py)
{
    _populate<Pin>(set, py, [&PCB](PyObject *arg){return PCB.getPin(arg);});
}

void populate(std::set<Component *> &set, PCBoard &PCB, PyObject *py)
{
    _populate<Component>(set, py, [&PCB](PyObject *arg){return PCB.getComponent(arg);});
}

} // namespace util

void PCBItemSets::clear()
{
    coms->clear();
    pins->clear();
    nets->clear();
    cons->clear();
}

void PCBItemSets::populate(PCBoard &PCB, PyObject *py)
{
    if (!PyDict_Check(py))
        throw std::invalid_argument("object set specifier must be a dict");
    auto C = PyDict_GetItemString(py, "components");
    auto P = PyDict_GetItemString(py, "pins");
    auto N = PyDict_GetItemString(py, "nets");
    auto X = PyDict_GetItemString(py, "connections");
    if (C && coms)
        util::populate(*coms, PCB, C);
    if (P && pins)
        util::populate(*pins, PCB, P);
    if (N && nets)
        util::populate(*nets, PCB, N);
    if (X && cons)
        util::populate(*cons, PCB, X);
}

void PCBItemSets::populatePinsFromComs()
{
    for (auto C : *coms)
        for (auto I : C->getPins())
            if (auto P = I->as<Pin>())
                pins->insert(P);
}
void PCBItemSets::populatePinsFromNets()
{
    for (auto net : *nets)
        pins->insert(net->getPins().begin(), net->getPins().end());
}
void PCBItemSets::populatePinsFromConnections()
{
    for (auto X : *cons) {
        if (X->sourcePin())
            pins->insert(X->sourcePin());
        if (X->targetPin())
            pins->insert(X->targetPin());
    }
}

void PCBItemSets::populateComsFromPins()
{
    for (auto P : *pins)
        if (auto C = P->getParent()->as<Component>())
            coms->insert(C);
}
void PCBItemSets::populateComsFromNets()
{
    for (auto net : *nets)
        for (auto P : net->getPins())
            if (auto C = P->getParent()->as<Component>())
                coms->insert(C);
}
void PCBItemSets::populateComsFromConnections()
{
    for (auto X : *cons) {
        if (X->sourcePin())
            if (auto C = X->sourcePin()->getParent()->as<Component>())
                coms->insert(C);
        if (X->targetPin())
            if (auto C = X->targetPin()->getParent()->as<Component>())
                coms->insert(C);
    }
}

void PCBItemSets::populateNetsFromComs()
{
    for (auto C : *coms)
        for (auto I : C->getPins())
            if (auto P = I->as<Pin>())
                if (P->hasNet())
                    nets->insert(P->net());
}
void PCBItemSets::populateNetsFromPins()
{
    for (auto P : *pins)
        if (P->hasNet())
            nets->insert(P->net());
}
void PCBItemSets::populateNetsFromConnections()
{
    for (auto X : *cons)
        if (X->hasNet())
            nets->insert(X->net());
}

void PCBItemSets::populateConnectionsFromNets()
{
    for (auto net : *nets)
        cons->insert(net->connections().begin(), net->connections().end());
}
void PCBItemSets::populateConnectionsFromPins()
{
    for (auto P : *pins)
        cons->insert(P->connections().begin(), P->connections().end());
}
void PCBItemSets::populateConnectionsFromComs()
{
    for (const auto *C : *coms)
        C->gatherConnections(*cons);
}

void PCBItemSets::prune(PCBoard &PCB)
{
    PCB.pruneConnections(*cons, false);
    PCB.pruneNets(*nets);
    PCB.prunePins(*pins);
    PCB.pruneComponents(*coms);
}

void PCBItemSets::complement(PCBoard &PCB)
{
    PCBItemSets that;
    for (auto C : PCB.getComponents()) {
        if (!coms->contains(C))
            that._coms.insert(C);
        for (auto I : C->getPins())
            if (auto P = I->as<Pin>())
                if (!pins->contains(P))
                    that._pins.insert(P);
    }
    for (auto N : PCB.getNets()) {
        if (!nets->contains(N))
            that._nets.insert(N);
        for (auto X : N->connections())
            if (!cons->contains(X))
                that._cons.insert(X);
    }
    _coms = std::move(that._coms);
    _pins = std::move(that._pins);
    _nets = std::move(that._nets);
    _cons = std::move(that._cons);
    coms = &_coms;
    pins = &_pins;
    nets = &_nets;
    cons = &_cons;
}
