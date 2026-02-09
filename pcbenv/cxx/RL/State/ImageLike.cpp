#include "Log.hpp"
#include "RL/State/ImageLike.hpp"
#include "PCBoard.hpp"
#include "Net.hpp"
#include "NavImage.hpp"

namespace sreps {

class ImageRasterize : public Image
{
public:
    ImageRasterize(uint w, uint h) : Image(w, h) { }
    ImageRasterize(PyObject *args) : Image(args) { }
    const char *name() const override { return "image_draw"; }
    PyObject *getPy(PyObject *args) override;
private:
    void updateStatic();
    std::unique_ptr<NavImage<uint8_t>> mStaticImage;
};

class ImageDownscale : public Image
{
public:
    ImageDownscale(uint w, uint h) : Image(w, h) { }
    ImageDownscale(PyObject *args) : Image(args) { }
    const char *name() const override { return "image_grid"; }
    PyObject *getPy(PyObject *args) override;
};

void Image::init(PCBoard &PCB)
{
    StateRepresentation::init(PCB);
    mImageBox = mPCB->getNavGrid().getBbox();
    updateView();
}
void Image::setParameters(PyObject *py)
{
    py::Object args(py);
    if (!args.isDict())
        return;
    auto bbox = args.item("bbox");
    auto crop = args.item("crop_auto");
    auto size = args.item("size");
    auto max_size = args.item("max_size");
    auto max_scale = args.item("max_scale");
    auto min_scale = args.item("min_scale");
    if (bbox)
        mImageBox = bbox.asBbox_2();
    if (crop)
        mAutoCrop = crop.isNumber() ? crop.toDouble() : (crop.asBool() ? 0.0f : -1.0f);
    if (size)
        mSize = size.asIVector_2("image size must be (int,int)");
    if (max_size)
        mSizeMax = max_size.asIVector_2("max image size must be (int,int)");
    if (max_scale)
        mScaleMax = max_scale.toDouble();
    if (min_scale)
        mScaleMin = min_scale.toDouble();
    checkParameters();
}
void Image::checkParameters()
{
    if (mScaleMin < 0.0f || mScaleMin > 1.0f)
        throw std::invalid_argument("minimum image scale must be >= 0 and <= 1");
    if (mScaleMax <= 0.0f)
        throw std::invalid_argument("maximum image scale must be > 0");
    if (mScaleMax < mScaleMin)
        throw std::invalid_argument(fmt::format("maximum image scale {} must be <= minimum image scale {}", mScaleMax, mScaleMin));
    if (mSize.x < 1 || mSize.y < 1)
        throw std::invalid_argument("image size must be >= 1");
}
void Image::autoCrop()
{
    if (!mPCB)
        return;
    assert(mAutoCrop >= 0.0f);
    Bbox_2 bbox = mPCB->getActiveAreaBbox();
    for (const auto N : mPCB->getNets())
        for (const auto X : N->connections())
            bbox += X->tracksBbox();
    mImageBox = geo::bbox_intersection(geo::bbox_expanded_rel(bbox, mAutoCrop), mPCB->getLayoutArea().bbox());
}
void Image::setZeroSpacings()
{
    NavSpacings svoid;
    svoid.Clearance = 0.0;
    svoid.TrackWidthHalf = 0.0;
    svoid.ViaRadius = 0.0;
    mPCB->getNavGrid().setSpacings(svoid);
}
void Image::updateView(const Bbox_2 *vbox)
{
    if (vbox)
        mImageBox = *vbox;
    else if (mAutoCrop >= 0.0f)
        autoCrop();
    if (!mSizeMax.x)
        return;
    const auto w = float(mImageBox.xmax() - mImageBox.xmin());
    const auto h = float(mImageBox.ymax() - mImageBox.ymin());
    const auto sx = mSizeMax.x / std::ceil(w);
    const auto sy = mSizeMax.y / std::ceil(h);
    const auto s = std::min(std::min(sx, sy), mScaleMax);
    if (s > 0.0f && s < mScaleMin)
        throw std::invalid_argument(fmt::format("cannot fit area into image of size {}x{}: scale {} < {}", mSizeMax.x, mSizeMax.y, s, mScaleMin));
    mSize.x = std::round(s * w);
    mSize.y = std::round(s * h);

    DEBUG("Image::updateView bbox=(" << mImageBox << ") scale=" << s << " size=" << mSize.x << 'x' << mSize.y << " vbox=" << vbox);
}

PyObject *ImageRasterize::getPy(PyObject *args)
{
    assert(mPCB);
    if (!mPCB)
        return 0;
    setParameters(args);
    if (!mLockedView)
        updateView();
    updateStatic();
    NavImage<uint8_t> image(*mStaticImage.get());
    image.drawDynamic(*mPCB);
    return image.movePy();
}

PyObject *ImageDownscale::getPy(PyObject *args)
{
    assert(mPCB);
    if (!mPCB)
        return 0;
    setParameters(args);
    if (!mLockedView)
        updateView();
    const NavGrid &nav = mPCB->getNavGrid();
    NavImage<uint8_t> image(std::min(uint(mSize.x), nav.getSize(0)), std::min(uint(mSize.y), nav.getSize(1)), mImageBox, nav.getSize(2));
    setZeroSpacings();
    if (mScaleMax >= 1.0f && image.checkFit(nav) >= 0)
        image.draw1To1(nav);
    else
        image.drawDownscale(nav);
    image.drawRatsNest(*mPCB, false);
    return image.movePy();
}

void ImageRasterize::updateStatic()
{
    const NavGrid &nav = mPCB->getNavGrid();
    if (mStaticImage && mStaticImage->getBbox() == mImageBox && mStaticImage->getSize() == IVector_3(mSize, nav.getSize(2)))
        return;
    mStaticImage.reset(0);
    mStaticImage = std::make_unique<NavImage<uint8_t>>(mSize.x, mSize.y, mImageBox, nav.getSize(2));
    mStaticImage->drawStatic(*mPCB);
}

Image *Image::create(PyObject *py)
{
    py::Object args(py);

    if (!args.isDict())
        return 0;
    const auto name = args.item("class").asStringView();
    if (name.substr(0, 5) != "image")
        return 0;
    if (name == "image_draw")
        return new ImageRasterize(py);
    else if (name == "image_grid")
        return new ImageDownscale(py);
    throw std::invalid_argument("unknown image representation");
}
Image *Image::createDownscale(uint w, uint h)
{
    return new ImageDownscale(w, h);
}
Image *Image::createRasterize(uint w, uint h)
{
    return new ImageRasterize(w, h);
}

} // namespace sreps
