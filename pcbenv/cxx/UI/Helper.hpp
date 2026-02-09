
#ifndef GYM_PCB_UI_HELPER_H
#define GYM_PCB_UI_HELPER_H

#include <string>
#include "GLContext.hpp"

namespace helper
{
GLuint loadShaderPipeline(GLuint &vp, const char *vsPath, GLuint &fp, const char *fsPath);
}

#endif // GYM_PCB_UI_HELPER_H
