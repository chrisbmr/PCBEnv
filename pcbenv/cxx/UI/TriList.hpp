
#ifndef GYM_PCB_UI_TRILIST_H
#define GYM_PCB_UI_TRILIST_H

#include "GLContext.hpp"
#include "Color.hpp"
#include "Geometry.hpp"
#include <vector>

class Camera;

class TriList
{
public:
    TriList(const PCBoard&);
    ~TriList();
    void update(const PCBoard&);
    void setLineWidth(float w) { mLineWidthBase = w; }
    void draw(const Camera&);
private:
    uint mNumTris{0};
    float mLineWidthBase{1.0f};
    GLuint mVBO{0};
    GLuint mIBO{0};
    GLuint mVAO{0};

    static GLuint sVP, sFP;
    static GLuint sPP;
    static GLint sUniformFillAlpha;

    void createVAO();
    static void createShaders();
};

#endif // GYM_PCB_UI_TRILIST_H
