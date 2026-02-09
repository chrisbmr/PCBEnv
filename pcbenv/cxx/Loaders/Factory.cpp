
#include "Loaders/Factory.hpp"
#include "Log.hpp"
#include "Util/Util.hpp"
#include "Net.hpp"
#include "PCBoard.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

PCBFactory::PCBFactory()
{
}

void PCBFactory::loadOverrides()
{
    auto s = getenv("PCBENV_LOAD_FANOUT");
    if (s)
        mLoadFanout = util::is1TrueYesOrPlus(s);

    s = getenv("PCBENV_LOAD_TRACKS");
    if (s)
        mLoadTracksAll = util::is1TrueYesOrPlus(s);

    s = getenv("PCBENV_KEEP_TRACK_WIDTHS");
    if (s)
        mFixedTrackParams = !util::is1TrueYesOrPlus(s);

    s = getenv("PCBENV_CHECK_PIN_INTERSECTIONS");
    if (s)
        mCheckPinIntersections = util::is1TrueYesOrPlus(s);
}

bool PCBFactory::loadFile(const std::string &filePath)
{
    INFO("Loading PCB file: " << filePath);
    try {
        mData = util::loadFile(filePath);
    } catch (const std::runtime_error &e) {
        ERROR(e.what());
        return false;
    }
    if (mData.empty())
        ERROR("File " << filePath << " is empty");
    return !mData.empty();
}

PCBoard *PCBFactory::loadAndCreate(const std::string &filePath)
{
    if (!loadFile(filePath))
        return 0;
    auto rv = create();
    if (rv)
        rv->setSourceFilePath(filePath);
    return rv;
}
PCBoard *PCBFactory::create(const std::string &data)
{
    if (data.empty())
        return 0;
    mData = data;
    return create();
}
PCBoard *PCBFactory::create()
{
    loadOverrides();
    auto PCB = createDSN();
    if (PCB && mLockRoutedConnections)
        lockRoutedConnections(*PCB);
    return PCB;
}

void PCBFactory::lockRoutedConnections(PCBoard &PCB)
{
    for (const auto *N : PCB.getNets())
        for (auto X : N->connections())
            if (X->hasTracks())
                X->setLocked(true);
}

void PCBFactory::setLoadTracks(PyObject *py)
{
    py::Object arg(py);
    if (!arg)
        return;
    mLoadTracksAll = arg.isBool() && arg.asBool(0);
    if (arg.isBool())
        return;
    mLoadTracksNets = arg.asStringContainer<std::set<std::string>>();
}

void NetOverrides::set(std::map<std::string, Micrometers> &M1,
                       std::map<Micrometers, Micrometers> &M2, PyObject *py)
{
    py::Object m(py);
    auto k = m.at(0);
    auto v = Micrometers(m.at(1).toDouble());
    if (k.isString())
        M1[k.asString()] = v;
    else M2[Micrometers(k.toDouble())] = v;
}
void NetOverrides::load(PyObject *py)
{
    py::Object arg(py);
    if (!arg.isDict())
        throw std::runtime_error("net_overrides must be a dict");
    if (auto W = arg.item("trace_widths_um"))
        for (uint i = 0; i < W.listSize(); ++i)
            set(mNetTraceWidths, mTraceWidths, *W.item(i));
    if (auto D = arg.item("via_diameters_um"))
        for (uint i = 0; i < D.listSize(); ++i)
            set(mNetViaDiameters, mViaDiameters, *D.item(i));
    if (auto C = arg.item("clearances_um"))
        for (uint i = 0; i < C.listSize(); ++i)
            set(mNetClearances, mClearances, *C.item(i));
    INFO(str());
}
Micrometers NetOverrides::traceWidth(const std::string &net, Micrometers w) const
{
    if (auto I = mNetTraceWidths.find(net); I != mNetTraceWidths.end())
        return I->second;
    auto I = mTraceWidths.find(w);
    return (I != mTraceWidths.end()) ? I->second : w;
}
Micrometers NetOverrides::viaDiameter(const std::string &net, Micrometers d) const
{
   if (auto I = mNetViaDiameters.find(net); I != mNetViaDiameters.end())
        return I->second;
    auto I = mViaDiameters.find(d);
    return (I != mViaDiameters.end()) ? I->second : d;
}
Micrometers NetOverrides::clearance(const std::string &net, Micrometers c) const
{
    if (auto I = mNetClearances.find(net); I != mNetClearances.end())
        return I->second;
    auto I = mClearances.find(c);
    return (I != mClearances.end()) ? I->second : c;
}
std::string NetOverrides::str() const
{
    std::stringstream ss;
    ss << "Net overrides:";
    for (const auto &I : mNetTraceWidths)
        ss << std::endl << "Trace width override: " << I.first << " -> " << I.second;
    for (const auto &I : mTraceWidths)
        ss << std::endl << "Trace width override: " << I.first << " -> " << I.second;
    for (const auto &I : mNetViaDiameters)
        ss << std::endl << "Via diameter override: " << I.first << " -> " << I.second;
    for (const auto &I : mViaDiameters)
        ss << std::endl << "Via diameter override: " << I.first << " -> " << I.second;
    for (const auto &I : mNetClearances)
        ss << std::endl << "Clearance override: " << I.first << " -> " << I.second;
    for (const auto &I : mClearances)
        ss << std::endl << "Clearance override: " << I.first << " -> " << I.second;
    return ss.str();
}

void PCBFactory::setTopologies(PyObject *py)
{
    py::DictIterator I(py);
    while (I.next()) {
        NetTopology T;
        auto name = I.k.asString();
        auto list = I.v;
        if (!list.isList())
            throw std::runtime_error("topologies must be a dict of lists");
        T.Connections.resize(list.listSize());
        for (uint l = 0; l < list.listSize(); ++l) {
            auto line = list.item(l);
            if (!line.isList())
                throw std::runtime_error("topology must be a list of lists");
            for (uint i = 0; i < line.listSize(); ++i) {
                auto v = line.item(i);
                if (v.isString())
                    T.Connections[l].emplace_back(v.asString(0));
                else
                    T.Connections[l].emplace_back(v.asPoint_25());
            }
        }
        mTopologies[name] = std::move(T);
    }
}

void NetTopology::create(Net &net) const
{
    auto PCB = net.getBoard();
    for (auto &L : Connections) {
        for (uint i = 1; i < L.size(); ++i) {
            auto &A = L[i-1];
            auto &B = L[i];
            auto T0 = std::holds_alternative<std::string>(A) ? PCB->getPin(std::get<std::string>(A)) : 0;
            auto T1 = std::holds_alternative<std::string>(B) ? PCB->getPin(std::get<std::string>(B)) : 0;
            auto v0 = T0 ? T0->getCenter25() : std::get<Point_25>(A);
            auto v1 = T1 ? T1->getCenter25() : std::get<Point_25>(B);
            net.addConnection(T0, v0, T1, v1);
        }
    }
}
