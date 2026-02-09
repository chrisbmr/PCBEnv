
#ifndef GYM_PCB_UI_RATSNEST_H
#define GYM_PCB_UI_RATSNEST_H

#include "GLContext.hpp"
#include "Color.hpp"
#include <map>
#include <vector>

class Connection;
class Bus;
class PCBoard;
class Camera;

class RatsNest
{
public:
    RatsNest(const PCBoard&);
    ~RatsNest();
    void setHL(const std::vector<Connection *> *);
    void update();
    void updateHL();
    void draw(const Camera&);
    void setLineWidth(float w) { mLineWidth = w; }
    float getLayerAlpha(uint z) const { return mLayerAlpha.at(z); }
    void setLayerAlpha(uint z, float a) { mLayerAlpha.at(z) = a; }
    void setColorHL(Color c);
    bool allVisible() const { return mAllVisible; }
    void setAllVisible(bool b) { mAllVisible = b; }
private:
    const PCBoard &mPCB;
    uint mNumLines{0};
    float mLineWidth{1.0f};
    GLfloat mColorVars[2][2][4];
    bool mAllVisible{true};
    std::map<const Connection *, std::pair<uint16_t, uint16_t>> mConn2Index;
    std::vector<uint16_t> mIndicesHL;
    std::vector<const Connection *> mHLConns;
    std::vector<float> mLayerAlpha;

    GLuint mVBO{0};
    GLuint mVAO{0};

    static GLuint sVP, sFP, sPP;
    static GLint sUniformColors;

    bool shouldAdd(const Connection&) const;
    float getLayerAlpha(const Pin *) const;

    void createVAO();
    static void createShaders();
};

#endif // GYM_PCB_UI_RATSNEST_H
