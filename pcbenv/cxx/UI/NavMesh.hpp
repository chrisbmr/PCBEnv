
#ifndef GYM_PCB_UI_NAVMESH_H
#define GYM_PCB_UI_NAVMESH_H

#include "GLContext.hpp"
#include "Color.hpp"
#include "Geometry.hpp"
#include <vector>

class Camera;
class PCBoard;
class NavPoint;

class NavMesh
{
public:
    NavMesh(const PCBoard&);
    ~NavMesh();
    void update(uint z);
    void setPointSize(float s) { mPointSizeBase = s; }
    void setColor(Color);
    void setAlpha(float a) { mColor4f[3] = a; }
    void draw(const Camera&);
    void setVisualizeAStar(bool b) { mVisualizeAStar = b; }
private:
    const PCBoard &mPCB;
    uint mNumPoints{0};
    float mPointSizeBase{1.0f};
    float mPointSize{1.0f};
    GLfloat mColor4f[4];
    GLuint mVBO{0};
    GLuint mVAO{0};
    bool mVisualizeAStar{false};

    static GLuint sVP, sFP;
    static GLuint sPP;
    static GLint sUniformMVP;
    static GLint sUniformPointSize;
    static GLint sUniformColor;

    Color navColor(const NavPoint&, uint seq);
    float navSize(const NavPoint&);

    void createVAO();
    static void createShaders();
};

#endif // GYM_PCB_UI_NAVMESH_H
