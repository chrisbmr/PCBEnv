#ifndef GYM_PCB_NAVTRIANGULATION_H
#define GYM_PCB_NAVTRIANGULATION_H

#include "Color.hpp"
#include "Geometry.hpp"

class PCBoard;
class Connection;

/**
 * Data associated with a triangle in the Delaunay triangulation of the board.
 */
class NavTri
{
public:
    const Triangle_2& getTriangle() const { return mTri; }
    const Color& getColor() const { return mColor; }
    void setVertices(const Point_2 *);
    void setColor(Color c) { mColor = c; }
private:
    Triangle_2 mTri;
    Color mColor;
};

/**
 * Interface to the Delaunay triangulation.
 */
class NavTriangulation
{
public:
    static NavTriangulation *create(PCBoard&);
    NavTriangulation(PCBoard &PCB) : mPCB(PCB) { }
    virtual ~NavTriangulation() { }

    void setLayer(int z) { mLayer = z; }
    void setPinsAsPoints() { mPinGeometry = 'c'; }
    void setPinsAsBoxes() { mPinGeometry = 'b'; }
    void setAddDisconnectedPins(bool b) { mAddDisconnectedPins = b; }
    void setAddFootprints(bool b) { mAddFootprints = b; }

    virtual bool build() = 0;
    virtual void insertRoute(const Connection&) = 0;
    virtual void removeRoute(const Connection&) = 0;

    const std::vector<NavTri>& getNavTris() const { return mNavTris; }
    virtual uint getNavIdx(const Point_2&, uint z) const = 0;
    NavTri& getNavTri(uint i) { return mNavTris[i]; }
    NavTri *getNavTri(const Point_2 &v, uint z);
    const NavTri& getNavTri(uint i) const { return mNavTris[i]; }
    const NavTri *getNavTri(const Point_2 &v, uint z) const;

    virtual bool findPathAStar(std::vector<NavTri *> &searchArea, Connection&) = 0;
    uint16_t incSearchSeq() { assert(mSearchSeq < 0xfffe); return ++mSearchSeq; }
    uint16_t getSearchSeq() const { return mSearchSeq; }
    virtual PyObject *getFaceGraphPy() const { return 0; }
    virtual PyObject *getLineGraphPy() const { return 0; }
protected:
    PCBoard &mPCB;
    std::vector<NavTri> mNavTris;
    int mLayer{0};
    uint16_t mSearchSeq{0};
    bool mAddDisconnectedPins{true};
    bool mAddFootprints{false};
    char mPinGeometry{'b'};
};

inline NavTri *NavTriangulation::getNavTri(const Point_2 &v, uint z)
{
    auto i = getNavIdx(v, z);
    if (i < mNavTris.size())
        return &mNavTris[i];
    return 0;
}
inline const NavTri *NavTriangulation::getNavTri(const Point_2 &v, uint z) const
{
    auto i = getNavIdx(v, z);
    if (i < mNavTris.size())
        return &mNavTris[i];
    return 0;
}

inline void NavTri::setVertices(const Point_2 *v)
{
    mTri = Triangle_2(v[0], v[1], v[2]);
}

#endif // GYM_PCB_NAVTRIANGULATION_H
