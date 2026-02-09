
#include "PyArray.hpp"
#include "NavImage.hpp"
#include "NavGrid.hpp"
#include "Connection.hpp"
#include "Net.hpp"
#include "PCBoard.hpp"
#include "RasterizerMidpoint.hpp"
#include <cstring>

namespace {

float rangeCoverage(int xi, float x0, float x1)
{
    assert((xi >= x0 || int(x0) == xi) && (xi <= x1));
    if (int(x0) == xi) return 1.0f - (x0 - xi);
    if (int(x1) == xi) return x1 - xi;
    return 1.0f;
}

} // anon namespace

template<typename chan_t> Real NavImage<chan_t>::computeEdgeLen(uint W, uint H, const Bbox_2 &bbox)
{
    const Real eh = (bbox.xmax() - bbox.xmin()) / W;
    const Real ev = (bbox.ymax() - bbox.ymin()) / H;
    // Square cells only, so make the larger one fit.
    return std::max(eh, ev);
}

template<typename chan_t> inline char NavImage<chan_t>::ZLabel(uint z) const
{
    if (z == 0) return 'T';
    if (z == mLayerB) return 'B';
    return 'M';
}

template<typename chan_t> inline int NavImage<chan_t>::checkFit(const UniformGrid25 &grid) const
{
    if (mSize[0] < grid.getSize(0) || mSize[1] < grid.getSize(1))
        return -1;
    if (mSize[0] > grid.getSize(0) || mSize[1] > grid.getSize(1))
        return 1;
    return 0;
}

template<typename chan_t> NavImage<chan_t>::NavImage(uint W, uint H, const Bbox_2 &bbox, uint D) : UniformGrid25(computeEdgeLen(W, H, bbox))
{
    mBbox = bbox;
    mSize[0] = W;
    mSize[1] = H;
    mSize[2] = 1;
    mStrideY = W;
    mStrideZ = 0;
    mLayerMCoverage = getLayerMCoverage(D);
    mLayerB = (D > 1) ? (D - 1) : 255;
    allocate();
}

template<typename chan_t> NavImage<chan_t>::NavImage(const NavImage &image) : UniformGrid25(image.EdgeLen)
{
    mBbox = image.mBbox;
    mSize[0] = image.mSize[0];
    mSize[1] = image.mSize[1];
    mSize[2] = image.mSize[2];
    mStrideY = image.mStrideY;
    mStrideZ = image.mStrideZ;
    mLayerMCoverage = image.mLayerMCoverage;
    mLayerB = image.mLayerB;
    allocate();
    if (mData && image.mData)
        std::memcpy(mData, image.mData, sizeInBytes());
}

template<typename chan_t> NavImage<chan_t>::NavImage(const NavGrid &nav) : NavImage(nav.getSize(0), nav.getSize(1), nav.getBbox(), nav.getSize(2))
{
    draw1To1(nav);
}

template<typename chan_t> NavImage<chan_t>::~NavImage()
{
    if (mData)
        std::free(mData);
}

template<typename chan_t> void NavImage<chan_t>::allocate()
{
    mData = static_cast<NavPixel<chan_t> *>(std::calloc(getNumPoints2D(), sizeof(NavPixel<chan_t>)));
    if (!mData)
        throw std::bad_alloc();
}

template<typename chan_t> chan_t NavImage<chan_t>::getLayerMCoverage(uint d)
{
    if (d < 2)
        return 0;
    d -= 2;
    if constexpr (std::is_same_v<chan_t, float>)
        return 1.0f / d;
    if constexpr (std::is_same_v<chan_t, uint8_t>)
        return uint(240.0f / d);
    static_assert(std::is_same_v<chan_t, float> || std::is_same_v<chan_t, uint8_t>);
}

template<typename chan_t> void NavImage<chan_t>::draw1To1(const NavGrid &nav)
{
    if (mSize[0] < nav.getSize(0) || mSize[1] < nav.getSize(1))
        throw std::runtime_error("Cannot draw NavGrid 1:1 to image of smaller size");
    if (mBbox != nav.getBbox())
        throw std::runtime_error("draw1To1() called with differing bounding boxes");
    for (uint y = 0; y < std::min(mSize[1], nav.getSize(1)); ++y) {
    for (uint x = 0; x < std::min(mSize[0], nav.getSize(0)); ++x) {
        for (uint z = 0; z < nav.getSize(2); ++z)
            at(x,y).addPoint(nav.getPoint(x,y,z), ZLabel(z), FullCoverage, mLayerMCoverage);
    }}
}
template<typename chan_t> void NavImage<chan_t>::drawDownscale(const NavGrid &nav)
{
    const uint SL = nav.XIndexBounded(mBbox.xmin());
    const uint SR = nav.XIndexBounded(mBbox.xmax());
    const uint SB = nav.YIndexBounded(mBbox.ymin());
    const uint ST = nav.YIndexBounded(mBbox.ymax());
    const uint WS = (SR - SL + 1);
    const uint HS = (ST - SB + 1);

    // (128,128) <- (512,256) then rh,v=(4,2) and W,HD=(128,64)
    const float rh = float(WS) / mSize[0];
    const float rv = float(HS) / mSize[1];
    const float ds = (rh > rv) ? rh : rv;
    if (ds < 1.0f)
        throw std::runtime_error("You are trying to upscale, don't!");
    const uint WD = (rh >= rv) ? mSize[0] : std::min(uint(WS / ds), mSize[0]);
    const uint HD = (rh >  rv) ? std::min(uint(HS / ds), mSize[1]) : mSize[1];

    DEBUG('(' << mSize[0] << 'x' << mSize[1] << ") <- (" << WS << 'x' << HS << ") r=" << ds << " targt region = (" << WD << 'x' << HD << ')');

    //const float cov = 1.0f / (ds * ds);
    const float McovF = NavImage<float>::getLayerMCoverage(nav.getSize(2));
    for (uint YD = 0; YD < HD; ++YD) {
    for (uint XD = 0; XD < WD; ++XD) {
        NavPixel<float> PF;
        PF.zero();
        float cov = 0.0f;
        const float xs0 = SL + XD * ds, xs1 = xs0 + ds;
        const float ys0 = SB + YD * ds, ys1 = ys0 + ds;
        for (uint YS = ys0; YS <= std::min(uint(ys1), ST); ++YS) {
        for (uint XS = xs0; XS <= std::min(uint(xs1), SR); ++XS) {
            const float Ly = rangeCoverage(YS, ys0, ys1);
            const float Lx = rangeCoverage(XS, xs0, xs1);
            cov += Lx * Ly;
            for (uint z = 0; z < nav.getSize(2); ++z)
                PF.addPoint(nav.getPoint(XS,YS,z), ZLabel(z), Lx * Ly, Lx * Ly * McovF);
        }}
        at(XD,YD).setFloat(PF, 1.0f/cov);
    }}
}
template<typename chan_t> void NavImage<chan_t>::drawRatsNest(const PCBoard &PCB, const bool all)
{
    for (const auto net : PCB.getNets())
        for (const auto *X : net->connections())
            if (all || !X->isRouted())
                drawRatsNest(*X);
}
template<typename chan_t> void NavImage<chan_t>::drawStatic(const PCBoard &PCB)
{
    for (const auto C : PCB.getComponents())
        for (const auto *P : C->getPins())
            draw(*(P->as<Pin>()));
}
template<typename chan_t> void NavImage<chan_t>::drawDynamic(const PCBoard &PCB)
{
    for (const auto net : PCB.getNets())
        for (const auto *X : net->connections())
            draw(*X);
}

namespace {
template<typename chan_t, uint num_chans, bool add> class NavPixAddROP final : public BaseROP
{
public:
    void writeIndex(uint i) const;
    void writeRangeZYX(uint Z0, uint Z1, uint Y0, uint Y1, uint X0, uint X1) override;
    void setImage(NavImage<chan_t> &image) { mImage = &image; }
    void setChan(uint i, uint c) { assert(i < num_chans); mChan[i] = c; }
    void setValue(chan_t v) { mValue = v; }
private:
    NavImage<chan_t> *mImage{0};
    uint mChan[num_chans];
    chan_t mValue{1};
};
template<typename chan_t, uint num_chans, bool add> void NavPixAddROP<chan_t, num_chans, add>::writeIndex(uint i) const
{
    // FIXME: Do we care that this is inefficient and we could just use separate ROPs for every function?
    for (uint c = 0; c < num_chans; ++c) {
        if (mChan[c] < NAV_IMAGE_NUM_CHANS) {
            if (add)
                mImage->at(i).Chan[mChan[c]] += mValue;
            else
                mImage->at(i).Chan[mChan[c]] = mValue;
        }
    }
}
template<typename chan_t, uint num_chans, bool add> void NavPixAddROP<chan_t, num_chans, add>::writeRangeZYX(uint Z0, uint Z1, uint Y0, uint Y1, uint X0, uint X1)
{
    for (uint Y = Y0; Y <= Y1; ++Y)
        for (uint I = mImage->LinearIndex(0, Y, X0); I <= mImage->LinearIndex(0, Y, X1); ++I)
            writeIndex(I);
}
} // anon namespace

template<typename chan_t> void NavImage<chan_t>::draw(const Connection &X)
{
    for (const auto *T : X.getTracks())
        draw(*T);
    if (!X.isRouted())
        drawRatsNest(X);
}
template<typename chan_t> void NavImage<chan_t>::draw(const Track &track)
{
    for (const auto &v : track.getVias())
        draw(v);
    for (const auto &s : track.getSegments())
        drawCopper(s.base());
}

template<typename chan_t> void NavImage<chan_t>::drawRatsNest(const Connection &X)
{
    char zmask = 0;
    const int T = 0;
    const int B = mLayerB;
    if (X.source().z() == T || (X.sourcePin() && X.sourcePin()->minLayer() == T)) zmask |= 1;
    if (X.source().z() == B || (X.sourcePin() && X.sourcePin()->maxLayer() == B)) zmask |= 2;
    if (X.target().z() == T || (X.targetPin() && X.targetPin()->minLayer() == T)) zmask |= 1;
    if (X.target().z() == B || (X.targetPin() && X.targetPin()->maxLayer() == B)) zmask |= 2;

    Rasterizer<NavPixAddROP<chan_t, 2, false>> R(*this);
    R.OP.setImage(*this);
    R.OP.setValue(FullCoverage);
    R.OP.setChan(0, (zmask & 1) ? NAV_IMAGE_CHAN_RATSNEST_T : NAV_IMAGE_NUM_CHANS);
    R.OP.setChan(1, (zmask & 2) ? NAV_IMAGE_CHAN_RATSNEST_B : NAV_IMAGE_NUM_CHANS);

    for (const auto &rat : X.getRatsNest())
        R.rasterizeLine(Segment_25(rat.first.xy(), rat.second.xy(), 0));
}
template<typename chan_t> void NavImage<chan_t>::draw(const Pin &T)
{
    Rasterizer<NavPixAddROP<chan_t, 2, true>> R(*this);
    R.OP.setImage(*this);
    R.OP.setValue(FullCoverage);
    R.OP.setChan(0, (T.minLayer() == 0) ? NAV_IMAGE_CHAN_PIN_T : NAV_IMAGE_NUM_CHANS);
    R.OP.setChan(1, (T.maxLayer() != 0) ? NAV_IMAGE_CHAN_PIN_B : NAV_IMAGE_NUM_CHANS);
    R.rasterizeFill(T.getShape(), 0, 0);
}
template<typename chan_t> void NavImage<chan_t>::drawCopper(const Segment_25 &s)
{
    Rasterizer<NavPixAddROP<chan_t, 1, true>> R(*this);
    R.OP.setImage(*this);
    R.OP.setValue((s.z() == 0 || s.z() == (int)mLayerB) ? FullCoverage : mLayerMCoverage);
    R.OP.setChan(0, (s.z() == 0) ? NAV_IMAGE_CHAN_TRACK_T : (s.z() == (int)mLayerB ? NAV_IMAGE_CHAN_TRACK_B : NAV_IMAGE_CHAN_TRACK_M));
    R.rasterizeLine(s, 0, 0);
}
template<typename chan_t> void NavImage<chan_t>::draw(const Via &V)
{
    Rasterizer<NavPixAddROP<chan_t, 1, true>> R(*this);
    R.OP.setImage(*this);
    R.OP.setValue(FullCoverage);
    R.OP.setChan(0, NAV_IMAGE_CHAN_VIA);
    R.rasterizeFill(V.getCircle(), 0, 0);
}

template<typename chan_t> PyObject *NavImage<chan_t>::movePy()
{
    static_assert(sizeof(NavPixel<chan_t>) == (NAV_IMAGE_NUM_CHANS * sizeof(chan_t)));
    auto v = mData;
    mData = 0;
    return py::NPArray<chan_t>(&v[0].Chan[0], mSize[1], mSize[0], NAV_IMAGE_NUM_CHANS).ownData().release();
}

template<typename chan_t> void NavPixel<chan_t>::setFloat(const NavPixel<float> &F, float cov)
{
    Chan[NAV_IMAGE_CHAN_TRACK_T] = F.Chan[NAV_IMAGE_CHAN_TRACK_T] * cov * 240.0f;
    Chan[NAV_IMAGE_CHAN_TRACK_M] = F.Chan[NAV_IMAGE_CHAN_TRACK_M] * cov * 240.0f;
    Chan[NAV_IMAGE_CHAN_TRACK_B] = F.Chan[NAV_IMAGE_CHAN_TRACK_B] * cov * 240.0f;

    Chan[NAV_IMAGE_CHAN_PIN_T] = F.Chan[NAV_IMAGE_CHAN_PIN_T] * 240.0f * cov;
    Chan[NAV_IMAGE_CHAN_PIN_B] = F.Chan[NAV_IMAGE_CHAN_PIN_B] * 240.0f * cov;

    Chan[NAV_IMAGE_CHAN_VIA] = F.Chan[NAV_IMAGE_CHAN_VIA] * cov * 240.0f;

    Chan[NAV_IMAGE_CHAN_RATSNEST_T] = F.Chan[NAV_IMAGE_CHAN_RATSNEST_T] ? 240 : 0;
    Chan[NAV_IMAGE_CHAN_RATSNEST_B] = F.Chan[NAV_IMAGE_CHAN_RATSNEST_B] ? 240 : 0;
}

template<typename chan_t> void NavPixel<chan_t>::addPoint(const NavPoint &nav, char z, chan_t vTB, chan_t vM)
{
    if (nav.hasFlags(NAV_POINT_FLAG_INSIDE_PIN | NAV_POINT_FLAG_PIN_TRACK_CLEARANCE))
        addPin(z, vTB);
    if (nav.hasFlags(NAV_POINT_FLAG_ROUTE_TRACK_CLEARANCE))
        addTrack(z, vTB, vM);
}
template<typename chan_t> void NavPixel<chan_t>::addPin(char z, chan_t v)
{
    if (z == 'T') Chan[NAV_IMAGE_CHAN_PIN_T] += v;
    if (z == 'B') Chan[NAV_IMAGE_CHAN_PIN_B] += v;
}
template<typename chan_t> void NavPixel<chan_t>::addRatsNest(char z, chan_t v)
{
    if (z == 'T') Chan[NAV_IMAGE_CHAN_RATSNEST_T] += v;
    if (z == 'B') Chan[NAV_IMAGE_CHAN_RATSNEST_B] += v;
}
template<typename chan_t> void NavPixel<chan_t>::addTrack(char z, chan_t vTB, chan_t vM)
{
    if (z == 'T') Chan[NAV_IMAGE_CHAN_TRACK_T] += vTB;
    if (z == 'B') Chan[NAV_IMAGE_CHAN_TRACK_B] += vTB;
    if (z == 'M') Chan[NAV_IMAGE_CHAN_TRACK_M] += vM;
}
template<typename chan_t> void NavPixel<chan_t>::addVia(chan_t v)
{
    Chan[NAV_IMAGE_CHAN_VIA] += v;
}

template<typename chan_t> void NavPixel<chan_t>::zero()
{
    std::memset(Chan, 0, sizeof(Chan));
}
template<typename chan_t> bool NavPixel<chan_t>::isNonzero() const
{
    for (auto v : Chan)
        if (v)
            return true;
    return false;
}

// We only need this one right now:
template class NavImage<uint8_t>;
