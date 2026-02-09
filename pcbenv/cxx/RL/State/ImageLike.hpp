#ifndef GYM_PCB_RL_STATE_IMAGELIKE_H
#define GYM_PCB_RL_STATE_IMAGELIKE_H

#include "RL/StateRepresentation.hpp"

namespace sreps {

/// Image returns the 8 channel NavImage.
class Image : public StateRepresentation
{
public:
    Image(int w, int h) : mSize{w,h} { }
    Image(PyObject *args) : mSize{128,128} { setParameters(args); }
    void init(PCBoard&) override;
    void updateView(const Bbox_2 * = 0) override;
    bool hasAutoCrop() const { return mAutoCrop; }
    void setAutoCrop(bool b) { mAutoCrop = b; }
    // PyObject *getPy(PyObject *connection_in_focus) override;
    static Image *create(PyObject *);
    static Image *createDownscale(uint w, uint h);
    static Image *createRasterize(uint w, uint h);
protected:
    IVector_2 mSizeMax{0,0}; // if this is non-zero, @size is calculated automatically
    IVector_2 mSize;
    float mScaleMax{1.0f};
    float mScaleMin{0.0f};
    Bbox_2 mImageBox;
    float mAutoCrop{-1.0f}; //< enabled if >= 0
    void setParameters(PyObject *);
    void checkParameters();
    void autoCrop();
    void setZeroSpacings();
};

} // namespace sreps

#endif // GYM_PCB_RL_STATE_IMAGELIKE_H
