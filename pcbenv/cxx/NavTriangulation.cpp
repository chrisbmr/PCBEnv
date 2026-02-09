
#include "PyArray.hpp"
#include "Log.hpp"
#include "NavTriangulation.hpp"
#include "PCBoard.hpp"
#include "Component.hpp"
#include "Net.hpp"
#include "Track.hpp"
#include "UserSettings.hpp"
#include "Math/Misc.hpp"
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/Triangulation_vertex_base_with_info_2.h>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <queue>
#include <type_traits>

/**
 * Information associated with each vertex of the triangulation.
 */
struct CDTVertexInfo
{
    const Connection *connection{0};
    const Pin *pin{0};
};

/**
 * Information associated with each face of the triangulation.
 * Contains node data for A-star.
 */
struct CDTFaceInfo
{
public:
    bool canRoute(const Connection&) const;
    bool hasParent() const { return !!mParent; }
    Object *getParent() const { return mParent; }
    bool hasPin() const { return mParent->is_a<Pin>(); }
    Pin *getPin() const { return mParent->as<Pin>(); }
    const Point_2& centroid() const { return mCentroid; }
    float score() const { return mScore; }
    float scoreEstimate() const { return mScoreEstimate; }
    bool isVisited(uint16_t seq) const { return mSeq == seq; }
    uint navIndex() const { return mNavTriIndex; }
    void setParent(Object *A) { mParent = A; }
    void setCentroid(const Point_2 v[3]) { mCentroid = CGAL::centroid(v[0], v[1], v[2]); }
    void setScores(float s, float est) { mScore = s; mScoreEstimate = est; }
    void setSeq(uint seq) { mSeq = seq; }
    void setNavIndex(uint i) { mNavTriIndex = i; }
    void setFrom(void *face) { mFrom = face; }
    void setEdgeIndex(uint i, uint e) { mEdgeIndex[i] = e; }
    uint getEdgeIndex(uint i) const { return mEdgeIndex[i]; }
    template<typename T> T *getFrom() const { return static_cast<T *>(mFrom); }
private:
    Object *mParent{0};
    void *mFrom{0};
    Point_2 mCentroid;
    float mScore;
    float mScoreEstimate;
    uint mEdgeIndex[3];
    uint mNavTriIndex{0};
    uint16_t mSeq{0};
};

typedef CGAL::Triangulation_vertex_base_with_info_2<CDTVertexInfo, Kernel> TVB2;
typedef CGAL::Triangulation_face_base_with_info_2<CDTFaceInfo, Kernel> TFB2;
typedef CGAL::Constrained_triangulation_face_base_2<Kernel, TFB2> CTFB2;
typedef CGAL::Triangulation_data_structure_2<TVB2, CTFB2> TDS2;
typedef CGAL::Exact_predicates_tag MyTag;
typedef CGAL::Constrained_Delaunay_triangulation_2<Kernel, TDS2, MyTag> Triangulation;

using Face_handle = Triangulation::Face_handle;
using Vertex_handle = Triangulation::Vertex_handle;
using Edge = Triangulation::Edge;

class NavCDT : public NavTriangulation
{
public:
    NavCDT(PCBoard&);
    bool build() override;
    void insertRoute(const Connection&) override;
    void removeRoute(const Connection&) override;
    bool findPathAStar(std::vector<NavTri *>&, Connection&) override;
    uint getNavIdx(const Point_2&, uint z) const override;

private:
    Triangulation TNG;
    NavGrid &mNav;
    std::multimap<const Connection *, Vertex_handle> mConnVertices;
    std::map<const Pin *, Vertex_handle> mPinVertices;
    uint mNumEdges;

private:
    NavTri& getTri(Face_handle F) { return mNavTris.at(F->info().navIndex()); }

    void addBoundingGrid(const Bbox_2 &area, Real dx, Real dy);
    void addComponent(const Component&);
    Vertex_handle addComponentPins(const Component&);
    Vertex_handle addPin(const Pin&);
    Vertex_handle addPinAsPoint(const Pin&);
    Vertex_handle addPinAsBox(const Pin&);
    void associateRectFaces(Vertex_handle v[4], const Object *);
    void eraseRoute(const Connection&);
    void collectFaces();
    Edge otherSide(const Edge&) const;

    bool astarReconstruct(std::vector<NavTri *>&, Connection&, Face_handle target, Face_handle source);

    static float approxDistance(Face_handle, const Point_2&);
};

NavCDT::NavCDT(PCBoard &PCB) : NavTriangulation(PCB), mNav(PCB.getNavGrid())
{
}

NavTriangulation *NavTriangulation::create(PCBoard &PCB)
{
    return new NavCDT(PCB);
}

inline uint NavCDT::getNavIdx(const Point_2 &v, uint z) const
{
    auto F = TNG.locate(v);
    if (F == 0)
        return mNavTris.size();
    return F->info().navIndex();
}

void NavCDT::addBoundingGrid(const Bbox_2 &area, Real dx, Real dy)
{
    if (dx < 1.0)
        dx *= area.xmax() - area.xmin();
    if (dy < 1.0)
        dx *= area.ymax() - area.ymin();
    TNG.insert(Point_2(area.xmin(), area.ymin()));
    TNG.insert(Point_2(area.xmin(), area.ymax()));
    TNG.insert(Point_2(area.xmax(), area.ymin()));
    TNG.insert(Point_2(area.xmax(), area.ymax()));
    for (auto x = area.xmin() + dx; dx > 0.0 && x < area.xmax(); x += dx) {
        TNG.insert(Point_2(x, area.ymin()));
        TNG.insert(Point_2(x, area.ymax()));
    }
    for (auto y = area.ymin() + dy; dy > 0.0 && y < area.ymax(); y += dy) {
        TNG.insert(Point_2(area.xmin(), y));
        TNG.insert(Point_2(area.xmax(), y));
    }
}

void NavCDT::associateRectFaces(Vertex_handle vh[4], const Object *A)
{
    // TODO: Is there a better way to get the interior faces?
    for (uint i = 0; i < 2; ++i) {
        auto F0 = TNG.incident_faces(vh[i]);
        auto F = F0;
        do {
            // If this face has 3 vertices of the rectangle, it is interior.
            uint n = 0;
            for (uint k = 1; k <= 3; ++k)
                n += F->has_vertex(vh[(i+k) & 3]) ? 1 : 0;
            assert(n < 3);
            if (n == 2)
                F->info().setParent(const_cast<Object *>(A));
        } while (++F != F0);
    }
}

Vertex_handle NavCDT::addPin(const Pin &T)
{
    if (!T.isOnLayer(mLayer))
        return 0;
    if (!mAddDisconnectedPins && !T.hasNet())
        return 0;
    if (mPinGeometry == 'b')
        return addPinAsBox(T);
    assert(mPinGeometry == 'c');
    return addPinAsPoint(T);
}
Vertex_handle NavCDT::addPinAsBox(const Pin &T)
{
    const auto &bbox = T.getBbox();
    Point_2 v[4];
    Vertex_handle vh[4];
    v[0] = Point_2(bbox.xmin(), bbox.ymin());
    v[1] = Point_2(bbox.xmax(), bbox.ymin());
    v[2] = Point_2(bbox.xmax(), bbox.ymax());
    v[3] = Point_2(bbox.xmin(), bbox.ymax());
    for (uint i = 0; i < 4; ++i)
        vh[i] = TNG.insert(v[i]);
    for (uint i = 0; i < 4; ++i)
        TNG.insert_constraint(vh[i], vh[(i+1) & 3]);
    associateRectFaces(vh, &T);
    return vh[0];
}
Vertex_handle NavCDT::addPinAsPoint(const Pin &T)
{
    auto vh = TNG.insert(mNav.getPoint(T.getCenter(), mLayer)->getRefPoint(&mNav));
    vh->info().pin = &T;
    mPinVertices[&T] = vh;
    return vh;
}

Vertex_handle NavCDT::addComponentPins(const Component &C)
{
    Vertex_handle rv = 0;
    for (auto T : C.getPins()) {
        auto vh = addPin(*T->as<Pin>());
        if (vh != 0)
            rv = vh;
    }
    return rv;
}
void NavCDT::addComponent(const Component &C)
{
    const Vertex_handle any_pin_vh = addComponentPins(C);
    if (!mAddFootprints)
        return;
    const auto &bbox = C.getBbox();
    Point_2 v[4];
    Vertex_handle vh[4];
    v[0] = Point_2(bbox.xmin(), bbox.ymin());
    v[1] = Point_2(bbox.xmax(), bbox.ymin());
    v[2] = Point_2(bbox.xmax(), bbox.ymax());
    v[3] = Point_2(bbox.xmin(), bbox.ymax());
    for (uint i = 0; i < 4; ++i)
        vh[i] = TNG.insert(v[i]);
    for (uint i = 0; i < 4; ++i)
        TNG.insert_constraint(vh[i], vh[(i+1) & 3]);

    if (any_pin_vh == 0) {
        associateRectFaces(vh, &C);
    } else {
        // Find triangles that are part of the component but not of a pin:
        // 1. Start with an arbitrary face connected to a pin vertex that is not part of the pin.
        // 2. Recursively add all neighbours through unconstrained edges.
        auto Ih = TNG.incident_faces(any_pin_vh);
        while (Ih->info().hasParent())
            Ih++;
        std::queue<Face_handle> queue;
        queue.push(Ih);
        while (!queue.empty()) {
            auto Fh = queue.front();
            queue.pop();
            for (uint i = 0; i < 3; ++i) {
                Face_handle Nh = Fh->neighbor(i);
                Edge e(Nh, i);
                if (!Nh->info().hasParent() && !TNG.is_constrained(e))
                    queue.push(Nh);
            }
            Fh->info().setParent(const_cast<Component *>(&C));
        }
    }
}

void NavCDT::removeRoute(const Connection &X)
{
    eraseRoute(X);
    collectFaces();
    mPCB.setChanged(PCB_CHANGED_NAV_TRIS);
}
void NavCDT::eraseRoute(const Connection &X)
{
    for (auto vh = mConnVertices.find(&X); vh != mConnVertices.end(); vh = mConnVertices.find(&X)) {
        TNG.remove_incident_constraints(vh->second);
        if (!vh->second->info().pin)
            TNG.remove(vh->second);
        mConnVertices.erase(vh);
    }
}
void NavCDT::insertRoute(const Connection &X)
{
    eraseRoute(X);
    std::vector<Vertex_handle> vh;
    for (const auto *T : X.getTracks()) {
    for (const auto &s : T->getSegments()) {
        if (s.z() != mLayer)
            continue;
        const auto v0 = s.source_2();
        const auto v1 = s.target_2();
        if (vh.empty() || vh.back()->point() != v0) {
            vh.push_back(TNG.insert(v0));
            vh.back()->info().connection = &X;
            mConnVertices.insert({ &X, vh.back() });
        }
        vh.push_back(TNG.insert(v1));
        vh.back()->info().connection = &X;
        mConnVertices.insert({ &X, vh.back() });
        TNG.insert_constraint(vh[vh.size() - 2], vh.back());
    }}
    collectFaces();
    mPCB.setChanged(PCB_CHANGED_NAV_TRIS);
}

bool NavCDT::build()
{
    addBoundingGrid(mPCB.getComponentAreaBbox(), 0, 0);
    for (auto C : mPCB.getComponents())
        addComponent(*C);
    collectFaces();
    INFO("Built constrained Delaunay triangulation with " << TNG.number_of_faces() << " faces.");
    return true;
}

Edge NavCDT::otherSide(const Edge &e) const
{
    const auto &F = e.first;
    const auto &s = e.second;
    const auto &N = e.first->neighbor(s);
    if (TNG.is_infinite(N))
        return e;
    const auto v0 = F->vertex(CGAL::Triangulation_cw_ccw_2::cw(s));
    const auto v1 = F->vertex(CGAL::Triangulation_cw_ccw_2::ccw(s));
    uint a;
    for (a = 0; a <= 2 && (N->vertex(a) == v0 || N->vertex(a) == v1); ++a);
    return Edge(N, a);
}

/**
 * Create a NavTri instance for each face of the triangulation.
 */
void NavCDT::collectFaces()
{
    // TODO: Can this be done incrementally?

    mNavTris.resize(TNG.number_of_faces());
    uint f = 0;
    for (const auto &face : TNG.finite_face_handles()) {
        Point_2 v[3];
        for (uint i = 0; i < 3; ++i)
            v[i] = face->vertex(i)->point();
        mNavTris[f].setVertices(v);
        mNavTris[f].setColor(face->info().hasParent() ? (face->info().getParent()->is_a<Pin>() ? Color::GREEN : Color::RED) : Color::WHITE);
        face->info().setCentroid(v);
        face->info().setNavIndex(f++);
    }

    mNumEdges = 0;
    for (const auto &EA : TNG.finite_edges()) {
        const auto EB = otherSide(EA);
        const auto sA = EA.second;
        const auto sB = EB.second;
        const auto &FA = EA.first;
        const auto &FB = EB.first;
        DEBUG("Face A " << &FA->info() << '/' << FA->info().navIndex() << " side " << sA << " == " << mNumEdges << " inf: " << TNG.is_infinite(FA));
        DEBUG("Face B " << &FB->info() << '/' << FB->info().navIndex() << " side " << sB << " == " << mNumEdges << " inf: " << TNG.is_infinite(FB));
        if (!TNG.is_infinite(FA))
            FA->info().setEdgeIndex(sA, mNumEdges);
        if (!TNG.is_infinite(FB))
            FB->info().setEdgeIndex(sB, mNumEdges);
        assert(!TNG.is_infinite(FA) || !TNG.is_infinite(FB));
        ++mNumEdges;
        if (TNG.is_infinite(FA) || TNG.is_infinite(FB))
            continue;
        assert(FA->vertex(CGAL::Triangulation_cw_ccw_2::cw(sA))->point()  == FB->vertex(CGAL::Triangulation_cw_ccw_2::cw(sB))->point() ||
               FA->vertex(CGAL::Triangulation_cw_ccw_2::cw(sA))->point()  == FB->vertex(CGAL::Triangulation_cw_ccw_2::ccw(sB))->point());
        assert(FA->vertex(CGAL::Triangulation_cw_ccw_2::ccw(sA))->point() == FB->vertex(CGAL::Triangulation_cw_ccw_2::cw(sB))->point() ||
               FA->vertex(CGAL::Triangulation_cw_ccw_2::ccw(sA))->point() == FB->vertex(CGAL::Triangulation_cw_ccw_2::ccw(sB))->point());
    }
}


// A* implementation.

bool CDTFaceInfo::canRoute(const Connection &X) const
{
    if (!hasParent())
        return true;
    const auto C = getParent();
    if (hasPin())
        return getPin()->net() == X.net();
    if (C->canRouteInside())
        return true;
    return X.sourceComponent() == C || X.targetComponent() == C;
}

class CDTFaceScoreLT {
public:
    bool operator()(Face_handle A, Face_handle B) { return A->info().scoreEstimate() >= B->info().scoreEstimate(); }
};

float NavCDT::approxDistance(Face_handle face, const Point_2& target)
{
    return std::sqrt(CGAL::squared_distance(face->info().centroid(), target));
}

bool NavCDT::findPathAStar(std::vector<NavTri *> &searchArea, Connection &route)
{
    // FIXME: This does not take into account routed tracks yet.

    auto sourceFace = TNG.locate(route.source().xy()); // TODO: Add hint for start of search.
    auto targetFace = TNG.locate(route.target().xy());
    if (sourceFace == 0 ||
        targetFace == 0)
        return false;

    const uint16_t seq1 = incSearchSeq();
    const uint16_t seq2 = incSearchSeq();

    const Real routeW2 = math::squared(route.defaultTraceWidth());

    DEBUG("NavCDT::findPathA* sourceFace[" << sourceFace->info().navIndex() << "]=" << getTri(sourceFace).getTriangle() << " targetFace[" << targetFace->info().navIndex() << "]=" << getTri(targetFace).getTriangle());

    sourceFace->info().setScores(0, approxDistance(sourceFace, route.target().xy()));
    sourceFace->info().setSeq(seq1);

    std::priority_queue<Face_handle, std::vector<Face_handle>, CDTFaceScoreLT> openList;
    openList.push(sourceFace);
    while (!openList.empty()) {
        auto curr = openList.top();
        openList.pop();
        if (!curr->info().isVisited(seq1))
            continue;
        curr->info().setSeq(seq2);
        if (curr == targetFace)
            return astarReconstruct(searchArea, route, curr, sourceFace);
        for (uint i = 0; i < 3; ++i) {
            auto node = curr->neighbor(i);
            if (TNG.is_infinite(node))
                continue;
            if (!node->info().canRoute(route))
                continue;
            const auto v1 = curr->vertex(CGAL::Triangulation_cw_ccw_2::cw(i));
            const auto v0 = curr->vertex(CGAL::Triangulation_cw_ccw_2::ccw(i));
            Real w2 = CGAL::squared_distance(v0->point(), v1->point());
            if (w2 < routeW2)
                continue;
            auto cost = CGAL::squared_distance(curr->info().centroid(), node->info().centroid());
            auto score = curr->info().score() + cost;
            if (node->info().isVisited(seq1) || node->info().isVisited(seq2))
                if (score >= node->info().score())
                    continue;
            node->info().setFrom(&*curr);
            node->info().setScores(score, score + approxDistance(node, route.target().xy()));
            if (!node->info().isVisited(seq1))
                node->info().setSeq(seq1);
            openList.push(node);
        }
#ifdef GYM_PCB_ENABLE_UI
        mNav.getPCB().setChanged(PCB_CHANGED_NAV_TRIS);
        UserSettings::get().sleepForActionDelay();
#endif
    }
    return false;
}

bool NavCDT::astarReconstruct(std::vector<NavTri *> &searchArea, Connection &route, Face_handle target, Face_handle source)
{
    Triangulation::Face *sourceFace = &*source;
    Triangulation::Face *face = &*target;
    searchArea.push_back(&mNavTris[face->info().navIndex()]);
    while (face != sourceFace) {
        auto prev = face->info().getFrom<Triangulation::Face>();
        assert(prev);
        face = prev;
        searchArea.push_back(&mNavTris[face->info().navIndex()]);
    }
    return true;
}
