
#ifndef GYM_PCB_UI_VIAPAINTER_H
#define GYM_PCB_UI_VIAPAINTER_H

#include "GLContext.hpp"

class PCBoard;
class Track;
class Via;
class Camera;

class ViaPainter
{
public:
    ViaPainter();
    ~ViaPainter();
    void update(const PCBoard&);
    void update(const Track&);
    void update(const std::vector<const Via *>&);
    void draw(const Camera&);
private:
    uint mNumVertices{0};

    GLuint mVBO{0};
    GLuint mVAO{0};

    static GLuint sVP, sFP, sPP;

    void createVAO();
    static void createShaders();
};

#endif // GYM_PCB_UI_VIAPAINTER_H
