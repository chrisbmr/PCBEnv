
#ifndef GYM_PCB_UI_NAVGRID_H
#define GYM_PCB_UI_NAVGRID_H

#include "GLContext.hpp"
#include "Color.hpp"
#include "Geometry.hpp"
#include <vector>

class Camera;
class PCBoard;

namespace ui {

class NavGrid
{
public:
    NavGrid(const PCBoard&);
    ~NavGrid();
    void update();
    void setLineSize(float s) { mLineSizeBase = s; }
    void setColor(Color);
    void draw(const Camera&);
private:
    const PCBoard &mPCB;
    uint mNumLines{0};
    float mLineSizeBase{0.25f};
    GLfloat mColor4f[4];
    GLuint mVBO{0};
    GLuint mVAO{0};

    static GLuint sVP, sFP;
    static GLuint sPP;
    static GLint sUniformColor;

    void getDrawRanges(const Camera&, uint start[2], uint count[2]);
    void createVAO();
    static void createShaders();
};

} // namespace ui

#endif // GYM_PCB_UI_NAVGRID_H
