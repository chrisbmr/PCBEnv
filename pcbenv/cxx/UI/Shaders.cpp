
#include "Py.hpp"
#include "Log.hpp"
#include "Shaders.hpp"
#include "GLContext.hpp"
#include "UserSettings.hpp"
#include "Helper.hpp"
#include "Util/Util.hpp"
#include <cassert>

Shaders *Shaders::sInstance = 0;

Shaders::Shaders()
{
    assert(!sInstance);
    sInstance = this;
}

Shaders::~Shaders()
{
    sInstance = 0;
    auto ctx = GL::getContext();
    for (auto &I : mTrove)
        ctx->glDeleteProgram(I.second);
}

GLuint Shaders::load(GLenum type, const std::string &fileName)
{
    auto I = mTrove.find(fileName);
    if (I != mTrove.end())
        return I->second;
    INFO("Loading shader " << fileName);
    auto src = util::loadFile(UserSettings::get().Paths.GLSL + fileName);
    const char *srcChars = src.c_str();
    auto ctx = GL::getContext();
    GLuint sp = ctx->glCreateShaderProgramv(type, 1, &srcChars);
    if (!validate(sp))
        throw std::runtime_error("Could not build shader.");
    mTrove[fileName] = sp;
    return sp;
}

void Shaders::forget(const std::string &fileName)
{
    auto I = mTrove.find(fileName);
    if (I == mTrove.end())
        return;
    GL::getContext()->glDeleteProgram(I->second);
    mTrove.erase(I);
}

bool Shaders::validate(GLuint id) const
{
    auto ctx = GL::getContext();
    GLint res;
    ctx->glGetProgramiv(id, GL_LINK_STATUS, &res);
    if (res)
        return true;
    GLchar log[1024];
    GLsizei size;
    ctx->glGetProgramInfoLog(id, sizeof(log) - 1, &size, log);
    log[size] = 0;
    ERROR("UI/Shaders: failed to link with reason \"" << log << '"');
    return false;
}
