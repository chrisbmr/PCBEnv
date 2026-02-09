#include "Log.hpp"
#include "PyArray.hpp"
#include "RL/State/Features.hpp"
#include "PCBoard.hpp"
#include "Net.hpp"
#include "Track.hpp"
#include "Util/Metrics.hpp"

namespace sreps {

struct BoardFeatures
{
    const PCBoard &PCB;
    std::vector<float> LayerBiasXY;
    std::vector<float> LayerFraction45;
    std::vector<float> LayerTrackLen;

    BoardFeatures(const PCBoard&);
    void sumLayerRoutingDirections();
    PyObject *getPy() const;
};
BoardFeatures::BoardFeatures(const PCBoard &_PCB) : PCB(_PCB)
{
    sumLayerRoutingDirections();
}
void BoardFeatures::sumLayerRoutingDirections()
{
    LayerBiasXY.assign(PCB.getNumLayers(), 0.0f);
    LayerFraction45.assign(PCB.getNumLayers(), 0.0f);
    LayerTrackLen.assign(PCB.getNumLayers(), 0.0f);
    for (const auto *N : PCB.getNets()) {
    for (const auto *X : N->connections()) { if (X->isLocked()) continue;
    for (const auto *T : X->getTracks()) {
    for (const auto &s : T->getSegments()) {
        const auto l = s.base().length();
        const auto z = s.z();
        switch (s.base().angle45()) {
        case  0: LayerBiasXY[z] += l; break;
        case 90: LayerBiasXY[z] -= l; break;
        case 45: LayerFraction45[z] += l; break;
        }
        LayerTrackLen[z] += l;
    }}}}
    for (uint z = 0; z < LayerBiasXY.size(); ++z) {
        if (LayerTrackLen[z] == 0.0f)
            continue;
        LayerBiasXY[z] = std::abs(LayerBiasXY[z] / (LayerTrackLen[z] - LayerFraction45[z]));
        LayerFraction45[z] /= LayerTrackLen[z];
    }
}
PyObject *BoardFeatures::getPy() const
{
    auto py = PyDict_New();
    if (!py)
        return 0;
    py::Object dict(py);
    dict.setItem("LayerBiasXY", FloatNPArray(LayerBiasXY).release());
    dict.setItem("LayerFraction45", FloatNPArray(LayerFraction45).release());
    dict.setItem("LayerTrackLen", FloatNPArray(LayerTrackLen).release());
    return py;
}

struct ConnectionFeatures
{
    Point_25 Coords[2];
    float Distance45;
    float DistanceFractionX;
    float MinTrackLen;
    float TrackLen;
    float TrackLenRatio;
    float NumVias;
    float FractionX;
    float Fraction45;
    float FractionY;
    float ExcessFractionX;
    float ExcessFractionY;
    float RMSD;
    float MaxDeviation;
    float FractionOutsideAABB;
    float IntersectionsRat;
    float IntersectionsTrack;
    float Winding;
    float Turning;
    float FreeSpaceAroundPin[2];
    float LayerUtilization;
    bool Routed;
    bool Locked;
    const Connection &X;
    const PCBoard &PCB;

    ConnectionFeatures(const PCBoard&, const Connection&);
    void initStatic();
    void initUnrouted();
    void initRouted();
    void sumTracks();
    void sumIntersections();
    void sumFreeSpaceAroundPins();
    float sumFreeSpaceAroundPin(const Pin *);
    float sumFreeSpaceAroundPin(const Pin&, uint z);
    PyObject *getPy() const;
};
ConnectionFeatures::ConnectionFeatures(const PCBoard &_PCB, const Connection &_X) : PCB(_PCB), X(_X)
{
    initStatic();
    if (Routed)
        initRouted();
    else
        initUnrouted();
    sumIntersections();
    sumFreeSpaceAroundPins();
}
void ConnectionFeatures::initStatic()
{
    const auto v = X.vector2();
    Coords[0] = X.source();
    Coords[1] = X.target();
    Distance45 = X.distance45();
    DistanceFractionX = std::abs(v.x()) / (std::abs(v.x()) + std::abs(v.y()));
    MinTrackLen = X.referenceLen();;
    Routed = X.isRouted();
    Locked = X.isLocked();
}
void ConnectionFeatures::initUnrouted()
{
    TrackLen = 0.0f;
    TrackLenRatio = 0.0f;
    NumVias = 0.0f;
    FractionX = 0.0f;
    Fraction45 = 0.0f;
    FractionY = 0.0f;
    ExcessFractionX = 0.0f;
    ExcessFractionY = 0.0f;
    RMSD = 0.0f;
    MaxDeviation = 0.0f;
    FractionOutsideAABB = 0.0f;
    Winding = 0.0f;
    Turning = 0.0f;
    LayerUtilization = 0.0f;
}
void ConnectionFeatures::initRouted()
{
    sumTracks();
}

void ConnectionFeatures::sumTracks()
{
    const IsoRectEx connBB(X.bbox());
    const Segment_2 connS2 = X.segment2();
    const Line_2 connL2 = connS2.supporting_line();
    const Vector_2 connDir = Segment_25(connS2).getDirection();

    assert(PCB.getNumLayers() <= 128);
    std::bitset<128> layersUsed;
    TrackLen = 0.0f;
    FractionX = 0.0f;
    Fraction45 = 0.0f;
    FractionY = 0.0f;
    ExcessFractionX = 0.0f;
    ExcessFractionY = 0.0f;
    FractionOutsideAABB = 0.0f;
    NumVias = 0.0f;
    RMSD = 0.0f;
    MaxDeviation = 0.0f;
    Winding = 0.0f;
    Turning = 0.0f;
    Real ProjLenSum = 0.0;
    for (const auto *T : X.getTracks()) {
        float h0;
        for (uint i = 0; i < T->numSegments(); ++i) {
            const auto &s = T->getSegment(i);
            switch (s.base().angle45()) {
            case 0: FractionX += s.base().length(); break;
            case 45: Fraction45 += s.base().length(); break;
            case 90: FractionY += s.base().length(); break;
            }
            ExcessFractionX += std::abs(s.base().s2().to_vector().x());
            ExcessFractionY += std::abs(s.base().s2().to_vector().y());
            FractionOutsideAABB += connBB.lengthOutside(s.base().s2());
            MaxDeviation = std::max(MaxDeviation, float(CGAL::squared_distance(s.source_2(), connS2)));
            MaxDeviation = std::max(MaxDeviation, float(CGAL::squared_distance(s.target_2(), connS2)));
            layersUsed.set(s.z());
            if (i == 0) {
                h0 = std::sqrt(CGAL::squared_distance(s.source_2(), connL2));
            } else {
                const auto A = s.base().angleWith(T->getSegment(i-1).base());
                Winding += A;
                Turning += std::abs(A);
            }
            const float dL = std::abs(CGAL::scalar_product(connDir, s.base().s2().to_vector()));;
            const float h1 = std::sqrt(CGAL::squared_distance(s.target_2(), connL2));
            RMSD += (h0 + h1) * 0.5 * dL;
            h0 = h1;
            ProjLenSum += dL;
        }
        TrackLen += T->length();
        NumVias += T->numVias();
    }
    // Normalize
    MaxDeviation = std::sqrt(MaxDeviation) / PCB.getLayoutArea().diagonalLength();
    TrackLenRatio = TrackLen / Distance45;
    LayerUtilization = layersUsed.count();
    LayerUtilization /= PCB.getNumLayers();
    ExcessFractionX = ExcessFractionX / std::abs(connS2.target().x() - connS2.source().x()) - 1.0f;
    ExcessFractionY = ExcessFractionY / std::abs(connS2.target().y() - connS2.source().y()) - 1.0f;
    if (TrackLen != 0.0f) {
        const auto r = 1.0f / TrackLen;
        FractionX *= r;
        Fraction45 *= r;
        FractionY *= r;
        assert(std::abs(FractionX + FractionY + Fraction45 - 1.0f) < 1e-3f);
        ExcessFractionX *= r;
        ExcessFractionY *= r;
        FractionOutsideAABB *= r;
    }
    if (ProjLenSum != 0.0)
        RMSD /= float(ProjLenSum);
}

void ConnectionFeatures::sumIntersections()
{
    uint nR = 0;
    uint nT = 0;
    for (const auto N : PCB.getNets()) {
        if (X.net() == N)
            continue;
        for (const auto Y : N->connections()) {
            if (X.intersects(*Y)) {
                if (Y->hasTracks())
                    nT += 1;
                else
                    nR += 1;
            }
        }
    }
    IntersectionsRat = float(nR);
    IntersectionsTrack = float(nT);
}

float ConnectionFeatures::sumFreeSpaceAroundPin(const Pin &T, uint z)
{
    const auto bbox = geo::bbox_expanded_rel(T.getBbox(), 1.0);
    const auto &nav = PCB.getNavGrid();
    const auto ibox = nav.getBox(bbox, z, z);
    int F = 0;
    int A = 0;
    for (int y = ibox.min.y; y <= ibox.max.y; ++y) {
    for (int x = ibox.min.x; x <= ibox.max.x; ++x) {
        const auto &P = nav.getPoint(x,y,z);
        if (!P.hasFlags(NAV_POINT_FLAG_INSIDE_PIN | NAV_POINT_FLAG_PIN_TRACK_CLEARANCE | NAV_POINT_FLAG_BLOCKED_PERMANENT)) {
            A += 1;
            if (!P.hasFlags(NAV_POINT_FLAGS_ROUTE_CLEARANCE))
                F += 1;
        }
    }}
    assert(A > 0 || F == 0);
    if (F == 0)
        return 0.0f;
    return float(F) / A;
}
float ConnectionFeatures::sumFreeSpaceAroundPin(const Pin *T)
{
    if (!T)
        return std::numeric_limits<float>::quiet_NaN();
    auto A = sumFreeSpaceAroundPin(*T, T->minLayer());
    if (T->maxLayer() != T->minLayer())
        A = std::max(A, sumFreeSpaceAroundPin(*T, T->maxLayer()));
    return A;
}
void ConnectionFeatures::sumFreeSpaceAroundPins()
{
    FreeSpaceAroundPin[0] = sumFreeSpaceAroundPin(X.sourcePin());
    FreeSpaceAroundPin[1] = sumFreeSpaceAroundPin(X.targetPin());
}

PyObject *ConnectionFeatures::getPy() const
{
    py::Object dict(PyDict_New());
    if (!dict)
        return 0;
    dict.setItem("Net", py::String(X.net()->name()));
    dict.setItem("Routed", Routed);
    dict.setItem("Locked", Locked);
    dict.setItem("Source", Coords[0]);
    dict.setItem("Target", Coords[1]);
    dict.setItem("Distance45", Distance45);
    dict.setItem("DistanceFractionX", DistanceFractionX);
    dict.setItem("MinTrackLen", MinTrackLen);
    dict.setItem("TrackLen", TrackLen);
    dict.setItem("TrackLenRatio", TrackLenRatio);
    dict.setItem("NumVias", NumVias);
    dict.setItem("FractionX", FractionX);
    dict.setItem("Fraction45", Fraction45);
    dict.setItem("FractionY", FractionY);
    dict.setItem("ExcessFractionX", ExcessFractionX);
    dict.setItem("ExcessFractionY", ExcessFractionY);
    dict.setItem("RMSD", RMSD);
    dict.setItem("MaxDeviation", MaxDeviation);
    dict.setItem("FractionOutsideAABB", FractionOutsideAABB);
    dict.setItem("IntersectionsRat", IntersectionsRat);
    dict.setItem("IntersectionsTrack", IntersectionsTrack);
    dict.setItem("Winding", Winding);
    dict.setItem("Turning", Turning);
    dict.setItem("PinSpace1", FreeSpaceAroundPin[0]);
    dict.setItem("PinSpace2", FreeSpaceAroundPin[1]);
    dict.setItem("LayerUtilization", LayerUtilization);
    return *dict;
}
void CustomFeatures::init(PCBoard &PCB)
{
    StateRepresentation::init(PCB);
    setFocus(0);
}
void CustomFeatures::setFocus(const std::vector<Connection *> *connections)
{
    mConnections.clear();
    if (connections) {
        mConnections = *connections;
    } else {
        for (auto N : mPCB->getNets())
            for (auto X : N->connections())
                if (!X->isLocked())
                    mConnections.push_back(X);
    }
}
PyObject *CustomFeatures::getPy(PyObject *_args)
{
    py::Object args(_args);
    if (!args || (args.isDict() && !args.item("as_dict").toBool()))
        return getArray();
    return getDict();
}
PyObject *CustomFeatures::getDict()
{
    py::Object dict(PyDict_New());
    for (const auto *X : mConnections)
        dict.setItem(X->name().c_str(), ConnectionFeatures(*mPCB, *X).getPy());
    dict.setItem("Board", BoardFeatures(*mPCB).getPy());
    return *dict;
}
PyObject *CustomFeatures::getArray()
{
    const uint D = 21;
    float *data = static_cast<float *>(malloc(D * (mConnections.size() + 3) * sizeof(float)));
    for (uint k = 0; k < mConnections.size(); ++k) {
        const ConnectionFeatures F(*mPCB, *mConnections[k]);
        const auto i = k + 3;
        data[i * D +  0] = F.Routed;
        data[i * D +  1] = F.TrackLen;
        data[i * D +  2] = F.TrackLenRatio;
        data[i * D +  3] = F.NumVias;
        data[i * D +  4] = F.FractionX;
        data[i * D +  5] = F.Fraction45;
        data[i * D +  6] = F.FractionY;
        data[i * D +  7] = F.ExcessFractionX;
        data[i * D +  8] = F.ExcessFractionY;
        data[i * D +  9] = F.RMSD;
        data[i * D + 10] = F.MaxDeviation;
        data[i * D + 11] = F.FractionOutsideAABB;
        data[i * D + 12] = F.IntersectionsRat;
        data[i * D + 13] = F.IntersectionsTrack;
        data[i * D + 14] = F.Winding;
        data[i * D + 15] = F.Turning;
        data[i * D + 16] = F.FreeSpaceAroundPin[0];
        data[i * D + 17] = F.FreeSpaceAroundPin[1];
        data[i * D + 18] = F.LayerUtilization;
        data[i * D + 19] = F.MinTrackLen;
        data[i * D + 20] = F.DistanceFractionX;
    }
    const BoardFeatures B(*mPCB);
    const uint L = B.LayerBiasXY.size();
    assert(B.LayerFraction45.size() == L &&
           B.LayerTrackLen.size() == L);
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    for (uint i = 0; i < D; ++i) {
        data[0 * D + i] = (i < L) ? B.LayerBiasXY[i] : nan;
        data[1 * D + i] = (i < L) ? B.LayerFraction45[i] : nan;
        data[2 * D + i] = (i < L) ? B.LayerTrackLen[i] : nan;
    }
    return FloatNPArray(data, mConnections.size() + 3, D).ownData().release();
}

} // namespace sreps
