
#ifndef GYM_PCB_UI_SHADERS_H
#define GYM_PCB_UI_SHADERS_H

#include <QOpenGLFunctions_4_3_Core>

class Shaders
{
public:
    Shaders();
    ~Shaders();
    static Shaders *getInstance() { assert(sInstance); return sInstance; }
    GLuint load(GLenum type, const std::string &name);
    bool validate(GLuint) const;
    void forget(const std::string &name);

private:
    std::map<std::string, GLuint> mTrove;

    static Shaders *sInstance;
};

#endif // GYM_PCB_UI_SHADERS_H
