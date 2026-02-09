#include "Log.hpp"
#include "UI/NavGrid.hpp"
#include "NavGrid.hpp"
#include "PCBoard.hpp"
#include "Camera.hpp"
#include "GLContext.hpp"
#include "Helper.hpp"
#include "Geometry.hpp"

namespace ui {

GLint NavGrid::sUniformColor = -1;
GLuint NavGrid::sVP = 0;
GLuint NavGrid::sFP = 0;
GLuint NavGrid::sPP = 0;

NavGrid::NavGrid(const PCBoard &pcb) : mPCB(pcb)
{
    createShaders();
    setColor(Color::WHITE);
    update();
}

NavGrid::~NavGrid()
{
    auto ctx = GL::getContext();
    if (mVBO)
        ctx->glDeleteBuffers(1, &mVBO);
    if (mVAO)
        ctx->glDeleteVertexArrays(1, &mVAO);
}

void NavGrid::getDrawRanges(const Camera &camera, uint start[2], uint count[2])
{
    const auto &nav = mPCB.getNavGrid();
    const auto size = IVector_2(nav.getSize(0), nav.getSize(1));
    auto I0 = nav.HIndices(camera.getCornerMin()).max_cw(IVector_2(0,0)).min_cw(size);
    auto I1 = nav.HIndices(camera.getCornerMax()).max_cw(IVector_2(0,0)).min_cw(size);
    start[0] = I0.x * 2;
    start[1] = I0.y * 2 + (size.x + 1) * 2;
    count[0] = (I1.x - I0.x + 1) * 2;
    count[1] = (I1.y - I0.y + 1) * 2;
    assert((start[1] + count[1]) <= mNumLines);
}

void NavGrid::update()
{
    const auto &nav = mPCB.getNavGrid();
    struct Vertex { float x, y; };
    std::vector<Vertex> data;
    INFO("UI/NavGrid: updated to size " << nav.getSize());
    data.resize(nav.getSize(0) * 2 + nav.getSize(1) * 2 + 4);
    uint i = 0;
    for (int xi = 0; xi <= int(nav.getSize(0)); ++xi, i += 2) {
        const Real x = nav.getBbox().xmin() + xi * nav.EdgeLen;
        assert((i + 1) < data.size());
        data[i+0].x = x;
        data[i+0].y = nav.getBbox().ymin();
        data[i+1].x = x;
        data[i+1].y = nav.getBbox().ymax();
    }
    for (int yi = 0; yi <= int(nav.getSize(1)); ++yi, i += 2) {
        const Real y = nav.getBbox().ymin() + yi * nav.EdgeLen;
        assert((i + 1) < data.size());
        data[i+0].x = nav.getBbox().xmin();
        data[i+0].y = y;
        data[i+1].x = nav.getBbox().xmax();
        data[i+1].y = y;
    }
    mNumLines = data.size();

    auto ctx = GL::getContext();
    if (!mVBO)
        ctx->glGenBuffers(1, &mVBO);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    ctx->glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * mNumLines, data.data(), GL_STATIC_DRAW);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, 0);
    createVAO();
}

void NavGrid::setColor(Color c)
{
    c.getFloat4(mColor4f);
    GL::getContext()->glProgramUniform4fv(sFP, sUniformColor, 1, mColor4f);
}

void NavGrid::draw(const Camera &camera)
{
    auto ctx = GL::getContext();
    auto lineSize = mLineSizeBase / std::sqrt(camera.getZoom());
    ctx->glBindProgramPipeline(sPP);
    ctx->glBindVertexArray(mVAO);
    if (mColor4f[3] < 1.0f)
        ctx->glEnable(GL_BLEND);
    if (lineSize != 1.0f)
        ctx->glLineWidth(lineSize);
#if 1
    uint start[2];
    uint count[2];
    getDrawRanges(camera, start, count);
    ctx->glDrawArrays(GL_LINES, start[0], count[0]);
    ctx->glDrawArrays(GL_LINES, start[1], count[1]);
#else
    ctx->glDrawArrays(GL_LINES, 0, mNumLines);
#endif
    if (lineSize != 1.0f)
        ctx->glLineWidth(1.0f);
    if (mColor4f[3] < 1.0f)
        ctx->glDisable(GL_BLEND);
    ctx->glBindVertexArray(0);
    ctx->glBindProgramPipeline(0);
}

void NavGrid::createVAO()
{
    if (mVAO)
        return;
    auto ctx = GL::getContext();
    ctx->glGenVertexArrays(1, &mVAO);
    ctx->glBindVertexArray(mVAO);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    ctx->glEnableVertexAttribArray(0);
    ctx->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, 0);
    ctx->glBindVertexArray(0);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void NavGrid::createShaders()
{
    sPP = helper::loadShaderPipeline(sVP, "basic.vs.glsl", sFP, "nav_grid.fs.glsl");

    auto ctx = GL::getContext();
    sUniformColor = ctx->glGetUniformLocation(sFP, "u_color");
    if (sUniformColor < 0)
        throw std::runtime_error("NavGrid: Could not get location of u_color.");
}

} // namespace ui
