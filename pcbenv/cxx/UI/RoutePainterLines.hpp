
#ifndef GYM_PCB_UI_ROUTEPAINTERLINES_H
#define GYM_PCB_UI_ROUTEPAINTERLINES_H

#include "GLContext.hpp"

class Connection;
class PCBoard;
class Camera;

class RoutePainterLines
{
public:
    RoutePainterLines(const PCBoard&);
    ~RoutePainterLines();
    void update(const std::vector<Connection *>&);
    void setLineWidth(float w) { mBaseLineWidth = w; }
    void draw(const Camera&);
private:
    const PCBoard &mPCB;
    uint mNumLineStrips{0};
    uint mVertexCountEstimate{64};
    float mBaseLineWidth{2.0f};
    float mColorVars[2][4];

    GLuint mVBO{0};
    GLuint mCMD{0};
    GLuint mVAO{0};

    static GLuint sVP, sFP, sPP;
    static GLint sUniformColors;

    void createVAO();
    static void createShaders();

    static constexpr const GLushort RestartIndex = 0xffff;
    static constexpr const bool UseLineStrips = false;
};

#endif // GYM_PCB_UI_ROUTEPAINTERLINES_H
