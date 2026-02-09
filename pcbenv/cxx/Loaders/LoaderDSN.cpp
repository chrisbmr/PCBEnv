
#include "Loaders/Factory.hpp"
#include "Loaders/DSN.hpp"
#include "Loaders/RouteTracker.hpp"
#include <cmath>
#include "Log.hpp"
#include "Geometry.hpp"
#include "PCBoard.hpp"
#include "Component.hpp"
#include "Net.hpp"
#include "Track.hpp"
#include "Math/Misc.hpp"
#include "Util/Util.hpp"

class SpecctraFactory
{
public:
    SpecctraFactory(const PCBFactory&);
    PCBoard *create();
    Real convertExtent(double) const;
    Point_2 getPoint(double x, double y) const;
    Point_25 getPoint(double x, double y, int z) const;
    Vector_2 getVector(double x, double y) const;
    SignalType convertLayerType(const std::string&) const;
private:
    Nanometers calibrateResolution() const;
    Micrometers unitsToMicroMeters(double) const;
    void createBoard();
    void createLayer(const dsn::Layer&);
    void createComponent(const dsn::Placement&);
    void createPin(Component *, const dsn::Pin&);
    void createNet(const dsn::Net&);
    SignalType getSignalType(const dsn::Net&) const;
    bool loadTracks(Net&, const dsn::Net&);
    void loadTrack(Connection&, const dsn::Track&);
    Circle_2 createCircle(const dsn::Shape&);
    WideSegment_25 createOval(const dsn::Path&);
    Polygon_2 createPolygon(const dsn::Path&);
    Polygon_2 createPolygon(const dsn::Shape&);
    Polygon_2 createPolygon(const std::vector<double>&);
    Iso_rectangle_2 createRect(const dsn::Shape&);
    bool checkIntersections(const Component *) const;
private:
    const PCBFactory &mFactory;
    dsn::Data mDSN;
    std::unique_ptr<PCBoard> mPCB;
    double mUnitToUM_Div;
    double mUnitToUM_Mul;
    bool mUnitMatchesResolution{false};
private:
    void createChannels();
    void createFakeComponent(const std::string &name, const Polygon_2&);
};

PCBoard *PCBFactory::createDSN()
{
    return SpecctraFactory(*this).create();
}
SpecctraFactory::SpecctraFactory(const PCBFactory &factory) : mFactory(factory)
{
}

inline Micrometers SpecctraFactory::unitsToMicroMeters(double v) const
{
    if (mDSN.pcb.unit_ex < -6)
        return Micrometers(v / mUnitToUM_Div);
    return Micrometers(v * mUnitToUM_Mul);
}
inline Real SpecctraFactory::convertExtent(double v) const
{
    if (mUnitMatchesResolution)
        return v;
    return mPCB->metersToUnits(unitsToMicroMeters(v)); // resolution units vs. design file units
}

Point_2 SpecctraFactory::getPoint(double x, double y) const
{
    return Point_2(convertExtent(x), convertExtent(y));
}
Point_25 SpecctraFactory::getPoint(double x, double y, int z) const
{
    return Point_25(getPoint(x, y), z);
}
Vector_2 SpecctraFactory::getVector(double x, double y) const
{
    return Vector_2(convertExtent(x), convertExtent(y));
}

Circle_2 SpecctraFactory::createCircle(const dsn::Shape &data)
{
    return Circle_2(Point_2(0, 0), math::squared(convertExtent(data.values[0] * 0.5)));
}
WideSegment_25 SpecctraFactory::createOval(const dsn::Path &data)
{
    if (data.xy.size() != 4)
        throw std::runtime_error("Oval path must have exactly 2 vertices");
    Point_2 v0 = getPoint(data.xy[0], data.xy[1]);
    Point_2 v1 = getPoint(data.xy[2], data.xy[3]);
    return WideSegment_25(v0, v1, 0, convertExtent(data.aperture_width * 0.5));
}
Polygon_2 SpecctraFactory::createPolygon(const dsn::Path &data)
{
    return createPolygon(data.xy);
}
Polygon_2 SpecctraFactory::createPolygon(const dsn::Shape &data)
{
    return createPolygon(data.values);
}
Polygon_2 SpecctraFactory::createPolygon(const std::vector<double> &xy)
{
    Polygon_2 poly;
    if (xy.size() & 1)
        throw std::runtime_error("Polygon with uneven number of values");
    for (uint i = 0; i < xy.size(); i += 2)
        poly.push_back(getPoint(xy[i], xy[i+1]));
    return poly;
}
Iso_rectangle_2 SpecctraFactory::createRect(const dsn::Shape &data)
{
    const Real x0 = data.values[0];
    const Real y0 = data.values[1];
    const Real x1 = data.values[2];
    const Real y1 = data.values[3];
    return Iso_rectangle_2(getPoint(x0, y0), getPoint(x1, y1));
}

void SpecctraFactory::createPin(Component *C, const dsn::Pin &data)
{
    const Vector_2 vec = getVector(data.x, data.y);
    const Point_2 pos = C->getRefPoint() + vec;
    DEBUG("* " << data.str() << " offset " << vec << " -> " << pos);
    int zmin = data.z0;
    int zmax = data.z1;
    for (const auto &S : data.padstack->shapes) {
        zmax = std::max(zmax, S.z);
        zmin = std::min(zmin, S.z);
    }
    if (zmin < 0 || uint(zmax) >= mPCB->getNumLayers()) {
        DEBUG("Pin " << C->name() << '-' << data.name << " is not on any copper layer, skipping");
        return;
    }
    Pin *P = C->addPin(data.name);
    for (const auto &S : data.padstack->shapes) {
        DEBUG(S.str(true));
        if (S.geometry == "circle") P->setShape(createCircle(S)); else
        if (S.geometry == "path") P->setShape(createOval(S.path), math::Radians(0.25)); else
        if (S.geometry == "polygon" && mFactory.loadPolygonsAsBoxes()) P->setShape(createPolygon(S).bbox()); else
        if (S.geometry == "polygon") P->setShape(createPolygon(S)); else
        if (S.geometry == "rect") P->setShape(createRect(S));
        else
            throw std::runtime_error(std::string("Unknown padstack shape: ") + S.geometry);
    }
    DEBUG("local bbox=" << P->getBbox());
    P->translate(Vector_2(pos.x(), pos.y()));
    if (data.angle != 0)
        P->rotateAround(pos, math::Radians(data.angle));
    assert(zmin >= 0 && uint(zmax) < mPCB->getNumLayers());
    DEBUG("final bbox=" << P->getBbox() << " z=[" << zmin << ',' << zmax << "]");
    P->setLayerRange(zmin, zmax);
    P->setClearance(convertExtent(data.clearance));
    // if (!mPCB->isInsideLayoutArea(*P)) // Might change after rotating component!
    //    throw std::runtime_error("Pin outside layout area");
}

void SpecctraFactory::createComponent(const dsn::Placement &data)
{
    DEBUG("New component: " << data.str());
    std::unique_ptr<Component> C(new Component(data.name));
    if (!C)
        return;
    C->setLocation(getPoint(data.pinref_x, data.pinref_y), data.z);
    if (data.image->footprint.geometry == "rect") {
        C->setShape(createRect(data.image->footprint));
    } else if (data.image->footprint.geometry == "polygon") {
        auto G = createPolygon(data.image->footprint);
        if (mFactory.loadPolygonsAsBoxes())
            C->setShape(G.bbox());
        else C->setShape(G);
    }
    if (C->getShape())
        C->translate(getVector(data.x, data.y));
    else if (data.image->pins.empty()) {
        DEBUG("Ignoring component " << C->name() << " as it has neither pins nor a footprint");
        return;
    }
    DEBUG("Reference point: " << C->getRefPoint() << " angle=" << data.angle);
    for (const auto &P : data.image->pins)
        createPin(C.get(), P);
    for (const auto &P : data.image->pins) {
        // Assemble pin compounds.
        if (!P.compound.has_value() || P.compound.value() == P.name)
            continue;
        auto PP = C->getPin(P.name);
        if (!PP)
            continue;
        auto KP = C->getPin(P.compound.value());
        if (!KP)
            throw std::runtime_error("invalid pin compound");
        PP->setCompound(KP);
    }
    if (C->numPins() && !C->getShape()) {
        Bbox_2 box = C->getPin(0).getBbox();
        for (uint i = 1; i < C->numPins(); ++i)
            box += C->getPin(i).getBbox();
        C->setShape(Iso_rectangle_2(box));
        DEBUG("pin bbox = " << box);
    }
    if (data.angle != 0 && C->getShape())
        C->rotateAround(getPoint(data.x, data.y), math::Radians(data.angle));
    C->setClearance(convertExtent(data.clearance));
    C->setCanRouteInside(data.can_route);
    C->setCanPlaceViasInside(data.via_ok);
    if (!mPCB->getLayoutArea().isBboxInside(*C)) {
        if (!mFactory.ignoreLayoutBounds())
            throw std::runtime_error("Component outside layout area");
        WARN("Component " << C->name() << " is outside of the layout area");
        mPCB->getLayoutArea().expand(C->getBbox());
    }
    if (mFactory.checkPinIntersections())
        if (checkIntersections(C.get()))
            throw std::runtime_error("intersecting pins found");
    mPCB->add(*C.release());
}

void SpecctraFactory::createFakeComponent(const std::string &name, const Polygon_2 &poly)
{
    std::unique_ptr<Component> C(new Component("FAKE"));
    C->setShape(poly);
    C->setLocation(C->getShape()->centroid(), 1);
    mPCB->add(*C.release());
}

void SpecctraFactory::loadTrack(Connection &X, const dsn::Track &data)
{
    auto T = X.newTrack(getPoint(data.start_x, data.start_y, data.start_z));
    T->setDefaultWidth(convertExtent(data.default_width));
    T->setDefaultViaDiameter(convertExtent(data.default_via_diameter));
    for (const auto &s : data.segments)
        T->_append(WideSegment_25(getPoint(s.x0, s.y0),
                                  getPoint(s.x1, s.y1), s.z, convertExtent(s.w * 0.5)));
    for (const auto &v : data.vias)
        T->_appendVia(getPoint(v.x, v.y), v.z0, v.z1, convertExtent(v.dia) * 0.5);
    T->setEnd(getPoint(data.end_x, data.end_y, data.end_z));
}

bool SpecctraFactory::loadTracks(Net &net, const dsn::Net &data)
{
    std::vector<WideSegment_25> segs;
    std::vector<Via> vias;
    for (auto &s : data.segments)
        segs.push_back(WideSegment_25(getPoint(s.x0, s.y0),
                                      getPoint(s.x1, s.y1), s.z,
                                      convertExtent(s.w * 0.5)).rectify(mFactory.getRectifySegmentsRadians()));
    for (auto &v : data.vias)
        vias.push_back(Via(getPoint(v.x, v.y), v.z0, v.z1, convertExtent(v.dia) * 0.5));
    return RouteTracker(net, segs, vias).run();
}

SignalType SpecctraFactory::getSignalType(const dsn::Net &net) const
{
    auto t = Net::getLikelySignalTypeForName(net.name);
    if (t != SignalType::SIGNAL)
        return t;
    const std::string klass = util::tolower(net.klass);
    if (klass == "power" || klass == "vin")
        return SignalType::POWER;
    if (klass == "ground")
        return SignalType::GROUND;
    return SignalType::SIGNAL;
}

void SpecctraFactory::createNet(const dsn::Net &data)
{
    if (data.pins.empty())
        WARN("Net \"" << data.name << "\" is empty");

    std::unique_ptr<Net> net(new Net(data.name));

    net->setSignalType(getSignalType(data));
    if (mFactory.hasFixedTrackParams()) {
        net->setMinTraceWidth(1);
        net->setViaDiameter(1);
        net->setMinClearance(0);
    } else {
        auto w = unitsToMicroMeters(data.width);
        auto d = unitsToMicroMeters(data.via_diameter);
        auto c = unitsToMicroMeters(data.clearance);
        const auto &over = mFactory.getNetOverrides();
        w = over.traceWidth(net->name(), w);
        d = over.viaDiameter(net->name(), d);
        c = over.clearance(net->name(), c);
        net->setMinTraceWidth(mPCB->metersToUnits(w));
        net->setViaDiameter(mPCB->metersToUnits(d));
        net->setMinClearance(mPCB->metersToUnits(c));
    }
    for (const auto &name : data.pins) {
        DEBUG("Net \"" << data.name << "\": adding pin " << name);
        auto P = mPCB->getPin(name);
        if (!P)
            throw std::runtime_error(fmt::format("Net {} references non-existent pin {}", data.name, name));
        net->insert(*P);
    }
    if (mFactory.getLoadTracksForNet(net->name()))
        loadTracks(*net, data);
    for (const auto &conn : data.connections) {
        auto P0 = conn.has_src_pin ? mPCB->getPin(conn.src_pin) : 0;
        auto P1 = conn.has_dst_pin ? mPCB->getPin(conn.dst_pin) : 0;
        if (P0 && P1 && (P0->net() != net.get() || P1->net() != net.get()))
            throw std::runtime_error("net contains invalid connection");
        if (P0 && P1 && net->getConnectionBetween(*P0, *P1))
            continue;
        if ((!P0 && !conn.has_src) || (!P1 && !conn.has_dst))
            throw std::runtime_error("connection has unspecified endpoint");
        const auto v0 = conn.has_src ? getPoint(conn.x0, conn.y0, conn.z0) : P0->getCenter25();
        const auto v1 = conn.has_dst ? getPoint(conn.x1, conn.y1, conn.z1) : P1->getCenter25();
        auto X = net->addConnection(P0, v0, P1, v1);
        X->setClearance(conn.clearance);
        for (const auto &track : conn.tracks)
            loadTrack(*X, track);
        X->checkRouted();
        if (conn.ref_len > 0.0)
            X->setReferenceLen(conn.ref_len);
    }
    if (net->numPins()) {
        const auto N = net.release();
        mPCB->add(*N);
        auto T = mFactory.getTopology(N->name());
        if (T)
            T->create(*N);
        if (mFactory.autocreateConnections())
            N->autocreateConnections();
    }
}

SignalType SpecctraFactory::convertLayerType(const std::string &s) const
{
    if (s == "signal" || s == "normal" || s == "mixed")
        return SignalType::SIGNAL;
    if (s == "ground")
        return SignalType::GROUND;
    return (s == "power") ? SignalType::POWER : SignalType::ANY;
}
void SpecctraFactory::createLayer(const dsn::Layer &data)
{
    DEBUG("Specctra " << data.str());
    assert((data.copper_index == 0) == (data.side == 't'));
    auto &L = mPCB->getLayer(data.copper_index);
    L.setType(convertLayerType(data.type));
    DEBUG(L.str());
}

void SpecctraFactory::createBoard()
{
    const Polygon_2 boundary = createPolygon(mDSN.structure.boundary);

    mPCB->setNumLayers(mDSN.structure.copper_layers.size());
    mPCB->getLayoutArea().setPolygonBbox(boundary);

    DEBUG("Layout area: " << mPCB->getLayoutArea().bbox());
}

Nanometers SpecctraFactory::calibrateResolution() const
{
    if (mFactory.getUnitLength_nm(Nanometers(0.0)).value() > 0.0)
        return mFactory.getUnitLength_nm(Nanometers(0.0));
    const double dsnLen = mDSN.pcb.resolution * std::pow(10, mDSN.pcb.resolution_ex + 6);
    if (mFactory.designResForced())
        return Nanometers(dsnLen * 1000);
    double D = mDSN.structure.boundary.bbox().maxExtent();
    double d = std::numeric_limits<double>::max();
    for (const auto &I : mDSN.padstacks)
        for (const auto &s : I.second.shapes)
            if (s.minExtent() > (D * 0x1.0p-16))
                d = std::min(d, s.minExtent());
    D = unitsToMicroMeters(D).value();
    d = unitsToMicroMeters(d).value();
    const uint nz = mDSN.structure.copper_layers.size();
    assert(d <= D);

    // Prefer at most 128x128x2 cells if the smallest pins will still have at least 4 cells.
    // Create no more than 1024x1024x8 cells.
    const double minLen = (D * nz) / 8192;
    const double refLen = (D * nz) / 256;
    const double pinLen = d / 2.0;
    double res = std::max(minLen, std::max(dsnLen, std::min(pinLen, refLen)));
    INFO("Adjusting resolution:\nDesign: " << dsnLen << " µm\nLimit(8192/NL): " << minLen << "µm\nGood(256/NL): " << refLen << "µm\nPins: " << pinLen << " µm\nUsing: " << res << " µm");
    return Nanometers(res * 1000);
}

PCBoard *SpecctraFactory::create()
{
    if (!dsn::Data::parseJSON(mDSN, mFactory.getData()) &&
        !dsn::Data::parseDSN(mDSN, mFactory.getData()) &&
        !dsn::Data::parseKiCAD(mDSN, mFactory.getData()))
        return 0;
    mDSN.inferMissingNetWidths();

    mUnitToUM_Mul = std::pow(10, mDSN.pcb.unit_ex + 6) * mDSN.pcb.unit;
    mUnitToUM_Div = std::pow(10, -6 - mDSN.pcb.unit_ex) / mDSN.pcb.unit;

    const Nanometers gridRes = calibrateResolution();
    mPCB = std::make_unique<PCBoard>(gridRes);
    if (!mPCB)
        return 0;
    mUnitMatchesResolution = gridRes.value() == (mUnitToUM_Mul * 1e3);
    INFO("Resolution: " << gridRes << (mUnitMatchesResolution ? " matches units" : ""));
    DEBUG(mDSN.pcb.str());
    DEBUG(mDSN.structure.str());
    mPCB->setName(mDSN.pcb.name);
    createBoard();
    for (const auto L : mDSN.structure.copper_layers)
        createLayer(*L);
    for (const auto &C : mDSN.placements)
        createComponent(C);
    for (const auto &N : mDSN.nets)
        createNet(N.second);
    if (Logger::get().visible(LogLevel::DEBUG))
        mDSN.printNetWidths();
    mPCB->adjustLayoutAreaMargins(mFactory.getLayoutAreaMinMargin(), mFactory.getLayoutAreaMaxMargin());
    mPCB->renumberPins();
    return mPCB.release();
}

bool SpecctraFactory::checkIntersections(const Component *C) const
{
    std::vector<std::pair<Object *, Object *>> pairs;
    for (const auto P1 : C->getPins())
        for (const auto P2 : C->getPins())
            if (P1 < P2 && P1->intersects(*P2))
                pairs.emplace_back(P1, P2);
    for (const auto C2 : mPCB->getComponents()) {
        if (!C->intersects(*C2))
            continue;
        for (const auto P1 : C->getPins())
            for (const auto P2 : C2->getPins())
                if (P1 < P2 && P1->intersects(*P2))
                    pairs.emplace_back(P1, P2);
    }
    for (auto &pair : pairs)
        WARN("pins " << pair.first->getFullName() << " & " << pair.second->getFullName() << " intersect");
    return !pairs.empty();
}
