#ifndef GYM_PCB_NAVIMAGE
#define GYM_PCB_NAVIMAGE

/// WARNING: We only draw tracks as 1 pixels wide lines without any coverage information.

#include "Rasterizer.hpp"

class PCBoard;
class NavGrid;
class NavPoint;
class Pin;
class Connection;
class Track;
class Via;

#define NAV_IMAGE_NUM_CHANS 8

#define NAV_IMAGE_CHAN_TRACK_T    0
#define NAV_IMAGE_CHAN_TRACK_B    3
#define NAV_IMAGE_CHAN_TRACK_M    6
#define NAV_IMAGE_CHAN_PIN_T      1
#define NAV_IMAGE_CHAN_PIN_B      4
#define NAV_IMAGE_CHAN_RATSNEST_T 2
#define NAV_IMAGE_CHAN_RATSNEST_B 5
#define NAV_IMAGE_CHAN_VIA        7

/**
 * This class represents a "pixel" in an "image" representation of the board.
 * If the encoding is u8, by default we use 0 to 240 for channels with graduation
 * (PIN_T,B and TRACK_M), and and 0/1 otherwise.
 * 240 is the most divisble by the common number of layers (1 to 6, 8, 10, 12, 15, 16).
 * This avoid roundoff errors when converting to floats for neural networks.
 */
template<typename chan_t> struct NavPixel
{
    chan_t Chan[NAV_IMAGE_NUM_CHANS];
    void setFloat(const NavPixel<float>&, float coverage);
    void addPoint(const NavPoint&, char z, chan_t vTB, chan_t vM);
    void addPin(char z, chan_t);
    void addRatsNest(char z, chan_t);
    void addTrack(char z, chan_t vTB, chan_t vM);
    void addVia(chan_t);
    void zero();
    bool isNonzero() const;
};

/**
 * This is usually a scaled down and/or cropped version of the NavGrid.
 * We use a 2D grid with layers encoded in the NavPixel's fixed number of channels.
 */
template<typename chan_t> class NavImage : public UniformGrid25
{
    constexpr static const chan_t FullCoverage = std::is_same_v<chan_t, float> ? 1 : 240;
public:
    NavImage(const NavImage<chan_t>&);
    NavImage(const NavGrid&);
    NavImage(uint w, uint h, const Bbox_2&, uint numLayers);
    ~NavImage();
    size_t sizeInBytes() const { return getNumPoints2D() * sizeof(NavPixel<chan_t>); }
    int checkFit(const UniformGrid25&) const; //!< -1/0/1 for </=/>

    NavPixel<chan_t>& at(uint i) { return mData[i]; }
    NavPixel<chan_t>& at(uint x, uint y) { return mData[y * mSize[0] + x]; }
    const NavPixel<chan_t>& at(uint x, uint y) const { return mData[y * mSize[0] + x]; }

    void draw1To1(const NavGrid&); //!< WARNING: draw1To1() does not set via channel
    void drawDownscale(const NavGrid&);
    void drawStatic(const PCBoard&);
    void drawDynamic(const PCBoard&);
    void drawRatsNest(const PCBoard&, const bool all);
    void draw(const Pin&);
    void draw(const Connection&);
    void draw(const Track&);
    void drawRatsNest(const Connection&);

    static chan_t getLayerMCoverage(uint d);

    PyObject *movePy(); //!< transfers ownership of mData to PyObject
private:
    NavPixel<chan_t> *mData;
    uint mLayerB;
    chan_t mLayerMCoverage; //!< e.g. (1 / number of inner layers)

private:
    void allocate();
    char ZLabel(uint z) const;

    static Real computeEdgeLen(uint w, uint h, const Bbox_2&);

    void draw(const Via&);
    void drawCopper(const Segment_25&);
};

#endif // GYM_PCB_NAVIMAGE
