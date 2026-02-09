
#ifndef GYM_PCB_BVH_H
#define GYM_PCB_BVH_H

#include "Geometry.hpp"
#include <set>

class Object;

class ObjectsBVH
{
public:
    static ObjectsBVH *create();
public:
    virtual ~ObjectsBVH() { }

    /**
     * Add objects within the bounding area to the set container.
     */
    virtual void select(std::set<Object *>&, const Bbox_2&, uint zmin = 0, uint zmax = ~0u) const = 0;

    /**
     * Select the deepest (child) object at the specified point.
     */
    virtual Object *select(const Point_2&, int z) const = 0;

    Object *select(const Point_25 &x) const { return select(x.xy(), x.z()); }

    virtual void insert(Object&) = 0;
};

#endif // GYM_PCB_BVH_H
