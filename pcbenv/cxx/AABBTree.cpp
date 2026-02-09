
#define BOOST_BIND_GLOBAL_PLACEHOLDERS

#include <CGAL/AABB_tree.h>
#if CGAL_VERSION_MAJOR >= 6
#include <CGAL/AABB_traits_2.h>
#include <CGAL/AABB_traits_3.h>
#else
#include <CGAL/AABB_traits.h>
#endif
#include <CGAL/Iso_cuboid_3.h>
#include <vector>
#include "Py.hpp"
#include "Log.hpp"
#include "BVH.hpp"
#include "PCBoard.hpp"
#include "Object.hpp"

using Iso_cuboid_3 = CGAL::Iso_cuboid_3<Kernel>;

struct Triangle_ref { const Object *Ref; uint Index; };

typedef std::vector<Triangle_ref>::const_iterator PCB_tri_iterator;

class PCB_triangle_primitive
{
public:
    typedef Triangle_ref Id;
    typedef Point_3 Point;
    typedef Triangle_3 Datum;

    PCB_triangle_primitive() { }
    PCB_triangle_primitive(PCB_tri_iterator I) : mRef(*I) { }

    const Id& id() const { return mRef; }

    Datum datum() const
    {
        const auto box = mRef.Ref->getBbox();
        auto x0 = box.xmin(), y0 = box.ymin();
        auto x1 = box.xmax(), y1 = box.ymax();
        auto z0 = mRef.Ref->minLayer();
        return mRef.Index ?
            Datum(Point_3(x0, y0, z0),
                  Point_3(x1, y1, z0),
                  Point_3(x0, y1, z0)) :
            Datum(Point_3(x0, y0, z0),
                  Point_3(x1, y0, z0),
                  Point_3(x1, y1, z0));
    }

    Point_3 reference_point() const
    {
        const auto box = mRef.Ref->getBbox();
        const auto z0 = mRef.Ref->minLayer();
        return mRef.Index ? Point_3(box.xmin(), box.ymax(), z0) : Point_3(box.xmax(), box.ymin(), z0);
    }

private:
    Id mRef;
};

#if CGAL_VERSION_MAJOR >= 6
typedef CGAL::AABB_traits_3<Kernel, PCB_triangle_primitive> PCB_tri_traits;
#else
typedef CGAL::AABB_traits<Kernel, PCB_triangle_primitive> PCB_tri_traits;
#endif
typedef CGAL::AABB_tree<PCB_tri_traits> Tri_AABB_tree;

class ObjectsAABBTree : public ObjectsBVH
{
public:
    ~ObjectsAABBTree() { }
    void insert(Object&) override;
    void select(std::set<Object *>&, const Bbox_2&, uint z0, uint z1) const override;
    Object *select(const Point_2&, int z) const override;
private:
    Tri_AABB_tree mTree;
};

ObjectsBVH *ObjectsBVH::create()
{
    return new ObjectsAABBTree();
}

void ObjectsAABBTree::insert(Object &A)
{
    DEBUG("Adding object " << A.getFullName() << " to BVH.");

    std::vector<Triangle_ref> tris;
    tris.resize(2);
    for (uint i = 0; i < 2; ++i) {
        tris[i].Ref = &A;
        tris[i].Index = i;
    }
    mTree.insert(tris.begin(), tris.end());
}

void ObjectsAABBTree::select(std::set<Object *> &res, const Bbox_2 &box, uint z0, uint z1) const
{
    Iso_cuboid_3 cube(Point_3(box.xmin(), box.ymin(), z0),
                      Point_3(box.xmax(), box.ymax(), z1));
    std::vector<Triangle_ref> primitives;
    mTree.all_intersected_primitives(cube, std::back_inserter(primitives));
    for (const auto &I : primitives)
        res.insert(const_cast<Object *>(I.Ref));
}

Object *ObjectsAABBTree::select(const Point_2 &x, int z) const
{
    std::vector<Triangle_ref> primitives;
    mTree.all_intersected_primitives(Point_3(x.x(), x.y(), z), std::back_inserter(primitives));
    if (primitives.empty())
        return 0;
    const Object *C1 = primitives[0].Ref;
    for (uint i = 1; i < primitives.size(); ++i) {
        auto C2 = primitives[i].Ref;
        if (C1->isContainerOf(*C2))
            C1 = C2;
    }
    return const_cast<Object *>(C1);
}
