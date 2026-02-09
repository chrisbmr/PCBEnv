
#include "TriList.hpp"
#include "Camera.hpp"
#include "GLContext.hpp"
#include "Helper.hpp"
#include "PCBoard.hpp"
#include "NavTriangulation.hpp"

GLint TriList::sUniformFillAlpha = -1;
GLuint TriList::sVP = 0;
GLuint TriList::sFP = 0;
GLuint TriList::sPP = 0;

#define USE_INDEX_BUFFER 0

TriList::TriList(const PCBoard &pcb)
{
    createShaders();
    update(pcb);
}

TriList::~TriList()
{
    auto ctx = GL::getContext();
    if (mVBO)
        ctx->glDeleteBuffers(1, &mVBO);
    if (mIBO)
        ctx->glDeleteBuffers(1, &mIBO);
    if (mVAO)
        ctx->glDeleteVertexArrays(1, &mVAO);
}

void TriList::update(const PCBoard &pcb)
{
    const auto TNG = pcb.getTNG();
    if (!TNG)
        return;

    struct Vertex { float x, y; uint32_t color; };
    std::vector<Vertex> data;
    std::vector<uint16_t> indices;
    for (const auto &navTri : TNG->getNavTris()) {
        const auto &tri = navTri.getTriangle();
        for (uint i = 0; i < 3; ++i) {
            Vertex v;
            v.x = tri.vertex(i).x();
            v.y = tri.vertex(i).y();
            v.color = navTri.getColor().ABGR8();
            data.push_back(v);
        }
    }
    mNumTris = data.size();

    auto ctx = GL::getContext();
    if (!mVBO)
        ctx->glGenBuffers(1, &mVBO);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    ctx->glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * mNumTris, data.data(), GL_STATIC_DRAW);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, 0);
    if (USE_INDEX_BUFFER) {
        ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIBO);
        ctx->glBufferData(GL_ELEMENT_ARRAY_BUFFER, 2 * indices.size(), indices.data(), GL_STATIC_DRAW);
        ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
    createVAO();
}

void TriList::draw(const Camera &camera)
{
    auto lineWidth = mLineWidthBase;
    if (camera.getZoom() < 1.0f / 64.0f)
        lineWidth = 4.0f;
    auto ctx = GL::getContext();
    ctx->glBindProgramPipeline(sPP);
    ctx->glBindVertexArray(mVAO);
    if (lineWidth != 1.0f)
        ctx->glLineWidth(lineWidth);
    ctx->glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    ctx->glProgramUniform1f(sFP, sUniformFillAlpha, 1.0f);
    ctx->glDrawArrays(GL_TRIANGLES, 0, mNumTris);
    ctx->glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    ctx->glProgramUniform1f(sFP, sUniformFillAlpha, 0.5f);
    ctx->glEnable(GL_BLEND);
    ctx->glDrawArrays(GL_TRIANGLES, 0, mNumTris);
    ctx->glDisable(GL_BLEND);
    ctx->glBindVertexArray(0);
    ctx->glBindProgramPipeline(0);
    if (lineWidth != 1.0f)
        ctx->glLineWidth(1.0f);
}

void TriList::createVAO()
{
    if (mVAO)
        return;
    auto ctx = GL::getContext();
    ctx->glGenVertexArrays(1, &mVAO);
    ctx->glBindVertexArray(mVAO);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    ctx->glEnableVertexAttribArray(0);
    ctx->glEnableVertexAttribArray(1);
    ctx->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 12, 0);
    ctx->glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 12, (void *)8);
    ctx->glBindVertexArray(0);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void TriList::createShaders()
{
    sPP = helper::loadShaderPipeline(sVP, "basic_color-flat.vs.glsl", sFP, "tri_list.fs.glsl");

    auto ctx = GL::getContext();
    sUniformFillAlpha = ctx->glGetUniformLocation(sFP, "u_FillAlpha");
    if (sUniformFillAlpha < 0)
        throw std::runtime_error("TriList: Could not get location of u_FillAlpha.");

    ctx->glProgramUniform1f(sFP, sUniformFillAlpha, 1.0f);
}
