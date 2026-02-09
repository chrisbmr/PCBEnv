
#include "Log.hpp"
#include "UserSettings.hpp"
#include "Camera.hpp"
#include "GLContext.hpp"

Camera::Camera()
{
    const auto conf = UserSettings::get().UI;

    setBaseDimensions(conf.WindowSize[0], conf.WindowSize[1], 1.0f);
}

Camera::~Camera()
{
    if (mUBO)
        GL::getContext()->glDeleteBuffers(1, &mUBO);
}

void Camera::update()
{
    mMVP.setOrtho(Vector_3(mCornerBL.x(), mCornerBL.y(), mZNear),
                  Vector_3(mCornerTR.x(), mCornerTR.y(), mZFar));
    for (uint i = 0; i < 16; ++i)
        mUniforms.MVP[i] = mMVP[i];

    mBbox = Bbox_2(mCornerBL.x(), mCornerBL.y(), mCornerTR.x(), mCornerTR.y());
}

void Camera::loadUniforms()
{
    auto ctx = GL::getContext();
    if (!mUBO)
        ctx->glGenBuffers(1, &mUBO);
    ctx->glBindBuffer(GL_UNIFORM_BUFFER, mUBO);
    ctx->glBufferData(GL_UNIFORM_BUFFER, sizeof(mUniforms), &mUniforms, GL_STATIC_DRAW);
    ctx->glBindBuffer(GL_UNIFORM_BUFFER, 0);

    ctx->glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUBO);
}

void Camera::setBaseDimensions(uint w, uint h, double unitsPerNanoMeter)
{
    const auto conf = UserSettings::get().UI;

    mBaseDims = Vector_2(w * conf.PointSize, h * conf.PointSize);
    setView(mCornerBL, mBaseDims * mZoom);

    INFO("UI/Camera: dimensions at zoom=1 set to " << mBaseDims.x() << 'x' << mBaseDims.y() << " board units encompassing " << mCornerBL << " to " << mCornerTR);

    update();
}

void Camera::setView(const Point_2 &origin, const Vector_2 &size)
{
    mZoom = std::max(size.y() / mBaseDims.y(),
                     size.x() / mBaseDims.x());
    mDims = mBaseDims * mZoom;
    setOrigin(origin);

    INFO("setView: origin=" << mCornerBL << " size=" << mDims << " zoom=" << mZoom);
}

void Camera::setZoom(float s, QPointF anchor)
{
    const Point_2 A = mCornerBL + Vector_2(mDims.x() * anchor.x(), mDims.y() * anchor.y());
    mZoom = s;
    mDims = mBaseDims * s;
    mCornerBL = A - Vector_2(mDims.x() * anchor.x(), mDims.y() * anchor.y());
    mCornerTR = mCornerBL + mDims;
    mCenter   = mCornerBL + mDims * 0.5;
    update();
}

void Camera::setCenter(const Point_2 &focus)
{
    mCenter = focus;
    mCornerBL = mCenter - mDims * 0.5;
    mCornerTR = mCenter + mDims * 0.5;
    update();
}
void Camera::setOrigin(const Point_2 &origin)
{
    INFO("setOrigin from " << mCornerBL << " to " << origin);

    mCornerBL = origin;
    mCenter   = origin + mDims * 0.5;
    mCornerTR = origin + mDims;
    update();
}
void Camera::setOriginRelative(const Vector_2 &v)
{
    INFO("UI/Camera: setOriginRelative " << v << ' ' << mDims);

    setOrigin(Point_2(mCornerBL.x() + v.x() * mDims.x(),
                      mCornerBL.y() + v.y() * mDims.y()));
}
void Camera::setZRange(float z0, float z1)
{
    mZNear = z0;
    mZFar = z1;
    update();
}
