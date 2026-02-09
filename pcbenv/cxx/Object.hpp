
#ifndef GYM_PCB_OBJECT_H
#define GYM_PCB_OBJECT_H

#include "AShape.hpp"
#include <string>

class CloneEnv;
class Via;

/**
 * An item on the board.
 * Either a Pin or a Component with pins as children.
 * Children are owned by the object and deleted by the destructor.
 */
class Object
{
protected:
    Object(Object *parent, const std::string &name);
public:
    virtual Object *clone(CloneEnv&) const { return 0; }
    virtual ~Object();

    uint id() const { return mId; }
    const std::string& name() const { return mName; }
    std::string getFullName() const; //!< {parent.name}-{name}

    void setId(uint id) { mId = id; }

    bool hasParent() const { return !!mParent; }
    Object *getParent() const { return mParent; }

    Object *getChildAt(const Point_2&) const;
    const std::vector<Object *>& getChildren() const { return mChildren; }
    uint numChildren() const { return mChildren.size(); }
    bool hasChildren() const { return !mChildren.empty(); }
    void addChild(Object&);
    void removeChild(Object&);

    bool isContainerOf(const Object&) const;

    virtual void addOccludedObject(Object&) { }

    /**
     * This is the center/centroid of the object's shape.
     * The layer is set to max(minLayer, parent.minLayer) so thru-hole pins from bottom-side components have z = bottom.
     */
    Point_25 getCenter25() const;
    const Point_2& getCenter() const { return mCenter; }
    const Bbox_2 getBbox() const { return mShape->bbox(); }

    /**
     * Call this only if you know this object only touches a single layer!
     */
    int getSingleLayer() const { assert(mLayerRange[0] == mLayerRange[1]); return mLayerRange[0]; }
    int minLayer() const { return mLayerRange[0]; }
    int maxLayer() const { return mLayerRange[1]; }
    bool isOnLayer(int z) const { return mLayerRange[0] <= z && z <= mLayerRange[1]; }
    bool sharesLayer(const Object&) const;
    bool sharesLayerRange(int zmin, int zmax) const;
    void setLayer(int);
    void setLayerRange(int zmin, int zmax);

    /**
     * Get the shape (with absolute positioning) of the object.
     */
    AShape *getShape() const { return mShape.get(); }

    /**
     * Set the shape (with absolute positioning) of the object.
     * This updates the object's center but not its children!
     */
    void setShape(const Circle_2&);
    void setShape(const Triangle_2&);
    void setShape(const Iso_rectangle_2&);
    void setShape(const Polygon_2&);
    void setShape(const WideSegment_25&, Real angleTolerance = 0.0);
    void setShape(Polygon_2&&);

    /**
     * Translate the object, including its children.
     */
    void translate(const Vector_2&);
    void transform(const Aff_transformation_2&);
    void rotateAround(const Point_2 &refPoint, Real angleRadians);

    Real getClearance() const { return mClearance; }
    void setClearance(Real c) { mClearance = c; }

    bool canRouteInside() const { return mCanRouteInside; }
    void setCanRouteInside(bool b) { mCanRouteInside = b; }
    bool canPlaceViasInside() const { return mCanPlaceViasInside; }
    void setCanPlaceViasInside(bool b) { mCanPlaceViasInside = b; }

    bool violatesClearance(const Via&, Real clearance, Point_25 * = 0) const;
    bool violatesClearance(const WideSegment_25&, Real clearance, Point_25 * = 0) const;

    Real distanceTo(const Point_2 &v) const { return std::sqrt(mShape->squared_distance(v)); }
    bool contains2D(const Point_2 &v) const { return mShape->contains(v); }
    bool contains3D(const Point_25 &v) const { return mShape->contains(v.xy()) && isOnLayer(v.z()); }
    bool isInsideBbox(const Point_2 &v) const;
    bool isInsideBbox(const Point_2 &v, int zmin, int zmax) const;
    bool intersects(const Bbox_2 &box) const { return mShape->intersects(box); }
    bool intersects(const WideSegment_25 &s) const { return isOnLayer(s.z()) && mShape->intersects(s); }
    bool intersects(const Object &A) const { return sharesLayer(A) && mShape->intersects(*A.getShape()); }
    bool bboxTest(const Bbox_2 &box, int z0, int z1 = -1); //!< z1 < 0 means z1=z0
    bool bboxTest(const Bbox_2 &box) { return CGAL::do_overlap(getBbox(), box); }

    /**
     * For UI use.
     */
    bool isSelected() const { return mSelectionFlag; }
    void setSelected(bool b) { mSelectionFlag = b; }
    void setSelected(bool b, uint recursion);

    void copyFrom(CloneEnv&, const Object&);

    virtual std::string str() const { return "?"; }
    virtual PyObject *getPy(uint depth) const;

    template<typename T> bool is_a() const { return !!dynamic_cast<const T *>(this); }
    template<typename T> const T *as() const { return dynamic_cast<const T *>(this); }
    template<typename T> T *as() { return dynamic_cast<T *>(this); }

protected:
    std::unique_ptr<AShape> mShape;
    Point_2 mCenter;
    Object *const mParent;
    std::vector<Object *> mChildren;
    std::string mName;
    uint mId;
    uint mChildIndex{std::numeric_limits<uint>::max()};
    int mLayerRange[2]{-1,-1};
    Real mClearance{0.0};
    bool mCanRouteInside{false};
    bool mCanPlaceViasInside{true};
    bool mSelectionFlag{false};

private:
    void update() { mCenter = mShape->centroid(); }
};

inline Point_25 Object::getCenter25() const
{
    int z = minLayer();
    if (hasParent())
        z = std::max(z, getParent()->minLayer());
    return Point_25(getCenter(), z);
}

inline bool Object::sharesLayerRange(int zmin, int zmax) const
{
    return minLayer() <= zmax && maxLayer() >= zmin;
}
inline bool Object::sharesLayer(const Object &A) const
{
    return sharesLayerRange(A.minLayer(), A.maxLayer());
}

inline bool Object::bboxTest(const Bbox_2 &B, int z0, int z1)
{
    if (z1 < 0)
        z1 = z0;
    return sharesLayerRange(z0, z1) && bboxTest(B);
}

#endif // GYM_PCB_OBJECT_H
