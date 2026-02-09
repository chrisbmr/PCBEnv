
#include "Helper.hpp"
#include "Shaders.hpp"

namespace helper
{

GLuint loadShaderPipeline(GLuint &vp, const char *vsPath, GLuint &fp, const char *fsPath)
{
    if (!vp)
        vp = Shaders::getInstance()->load(GL_VERTEX_SHADER, vsPath);
    if (!fp)
        fp = Shaders::getInstance()->load(GL_FRAGMENT_SHADER, fsPath);

    auto ctx = GL::getContext();
    GLuint pipe;
    ctx->glGenProgramPipelines(1, &pipe);
    ctx->glUseProgramStages(pipe, GL_VERTEX_SHADER_BIT, vp);
    ctx->glUseProgramStages(pipe, GL_FRAGMENT_SHADER_BIT, fp);

    GL::CheckErrors("loadShaderPipeline");
    return pipe;
}

} // namespace helper
