
#ifndef GYM_PCB_UI_CAMERA_H
#define GYM_PCB_UI_CAMERA_H

#include "Py.hpp"
#include "Math/Mat4.hpp"
#include "Math/Misc.hpp"
#include <QOpenGLFunctions_4_3_Core>
#include <QPointF>

/// Camera CLASS
/// Controls the view of the board.
/// Zoom: multiplier for view area, i.e. smaller values zoom in
class Camera
{
public:
    Camera();
    ~Camera();

    /// normalizedPos is 0 to 1 within the GL widget (= camera view area)
    QPointF worldPosToNormalizedPos(const Point_2&) const;
    Point_2 normalizedPosToWorldPos(const QPointF&) const;

    bool isVisible(const Bbox_2 &bbox) const { return CGAL::do_overlap(mBbox, bbox); }

    void loadUniforms();

    float adjustLineWidth(float) const;

    void setBaseDimensions(uint w, uint h, double unitsPerNanoMeter);
    void setZoom(float, QPointF anchor = QPointF(0.5, 0.5));
    void setOrigin(const Point_2&);
    void setOriginRelative(const Vector_2&);
    void setCenter(const Point_2&);
    void setView(const Point_2 &origin, const Vector_2 &size);
    void setZNear(float z);
    void setZFar(float z);
    void setZRange(float near, float far);

    void moveBy(const Vector_2 &v) { setCenter(mCenter + v); }

    const Bbox_2 getBbox() const { return mBbox; }
    const float getZoom() const { return mZoom; }
    const Vector_2& getDims() const { return mDims; }
    const Point_2& getCornerMin() const { return mCornerBL; }
    const Point_2& getCornerMax() const { return mCornerTR; }
    const Point_2& getOrigin() const { return mCornerBL; }

    const math::Mat4& getMVP() const { return mMVP; }

    const float *getFloatMVP() const { return &mUniforms.MVP[0]; }
    struct {
        GLfloat MVP[16];
    } mUniforms;

    GLuint getUBO() const { return mUBO; }

private:
    math::Mat4 mMVP;
    float mZoom{1.0f};
    Vector_2 mBaseDims;
    Vector_2 mDims;
    Bbox_2 mBbox;
    Point_2 mCornerBL{0,0};
    Point_2 mCornerTR;
    Point_2 mCenter;
    float mZNear{-1.0f};
    float mZFar{1.0f};
    //float mAspectRatio{1.0f};
    GLuint mUBO{0};

    void update();
};

inline void Camera::setZNear(float z)
{
    setZRange(z, mZFar);
}
inline void Camera::setZFar(float z)
{
    setZRange(mZNear, z);
}

inline Point_2 Camera::normalizedPosToWorldPos(const QPointF &v) const
{
    return mCornerBL + Vector_2(mDims.x() * v.x(), mDims.y() * v.y());
}
inline QPointF Camera::worldPosToNormalizedPos(const Point_2 &v) const
{
    auto dv = v - mCornerBL;
    return QPointF(float(dv.x()) / mDims.x(), 1.0f - float(dv.y()) / mDims.y());
}

inline float Camera::adjustLineWidth(float lw) const
{
    return lw * math::approx::rsqrt(mZoom);
}

#endif //  GYM_PCB_UI_CAMERA_H
