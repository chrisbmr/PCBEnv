
#ifndef GYM_PCB_UI_GLCONTEXT_H
#define GYM_PCB_UI_GLCONTEXT_H

#include "GLWidget.hpp"

namespace GL
{

inline GLWidget *getContext()
{
    return GLWidget::getInstance();
}

inline bool CheckErrors(const char *info)
{
    auto ctx = getContext();
    auto err = ctx->glGetError();
    if (err == GL_NO_ERROR)
        return false;
    do {
        switch (err) {
        default:
            qWarning("%s: GL_ERROR_%04x", info, err);
            break;
        case GL_INVALID_OPERATION: qWarning("%s: GL_INVALID_OPERATION", info); break;
        }
        err = ctx->glGetError();
    } while (err != GL_NO_ERROR);
    return true;
}

} // namespace GL

#endif // GYM_PCB_UI_GLCONTEXT_H
