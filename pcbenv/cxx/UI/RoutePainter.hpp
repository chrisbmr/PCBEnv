
#ifndef GYM_PCB_UI_ROUTEPAINTER_H
#define GYM_PCB_UI_ROUTEPAINTER_H

#include "GLContext.hpp"
#include "Color.hpp"

class PCBoard;
class Track;
class Camera;

class RoutePainter
{
public:
    constexpr static const uint MaxLayers = 16;
    RoutePainter();
    ~RoutePainter();
    void update(const PCBoard&);
    void update(const Track&, uint zmax, Color);
    void setLineWidth(float w) { mBaseLineWidth = w; }
    void draw(const Camera&);
    void drawLayer(const Camera&, uint z);
    void setLayerColor(uint z, Color);
    void setLayerAlpha(uint z, float);
    void clear();
private:
    //uint mNumTriStrips{0};
    uint mVertexCount{0};
    std::vector<uint> mIndexCounts;
    GLenum mIndexType;
    float mBaseLineWidth{2.0f};
    float mLayerColor[MaxLayers][4];

    GLuint mVBO{0};
    GLuint mIBO{0};
    GLuint mCMD{0};
    GLuint mVAO{0};

    static GLuint sVP, sFP, sPP;
    static GLint sUniformLayerColors;

    void createVAO();
    void createIBO();
    static void createShaders();

    static constexpr const GLushort RestartIndex = 0xffff;
};

#endif // GYM_PCB_UI_ROUTEPAINTER_H
