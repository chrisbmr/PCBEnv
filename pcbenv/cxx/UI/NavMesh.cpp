
#include "NavMesh.hpp"
#include "PCBoard.hpp"
#include "NavGrid.hpp"
#include "Camera.hpp"
#include "GLContext.hpp"
#include "Helper.hpp"
#include "Geometry.hpp"

GLint NavMesh::sUniformPointSize = -1;
GLint NavMesh::sUniformColor = -1;
GLuint NavMesh::sVP = 0;
GLuint NavMesh::sFP = 0;
GLuint NavMesh::sPP = 0;

NavMesh::NavMesh(const PCBoard &pcb) : mPCB(pcb)
{
    createShaders();
    setColor(Color::WHITE);
    setAlpha(0.75f);
    update(0);
}

NavMesh::~NavMesh()
{
    auto ctx = GL::getContext();
    if (mVBO)
        ctx->glDeleteBuffers(1, &mVBO);
    if (mVAO)
        ctx->glDeleteVertexArrays(1, &mVAO);
}

Color NavMesh::navColor(const NavPoint &v, uint seq)
{
    if (v.hasFlags(NAV_POINT_FLAG_ROUTE_GUARD))
        return Color::MAGENTA;
    if (v.hasFlags(NAV_POINT_FLAG_BLOCKED_TEMPORARY | NAV_POINT_FLAG_BLOCKED_PERMANENT))
        return Color::RED;
    if (mVisualizeAStar && v.getVisits().isSeen(seq))
        return v.getVisits().isDone(seq) ? Color::GREEN : Color::BLUE;
    if (v.hasFlags(NAV_POINT_FLAGS_TRACK_CLEARANCE))
        return Color::ORANGE;
    if (v.hasFlags(NAV_POINT_FLAGS_VIA_CLEARANCE))
        return Color::YELLOW;
    if (v.hasFlags(NAV_POINT_FLAG_NO_VIAS))
        return Color::CYAN;
    return Color::WHITE;
}
float NavMesh::navSize(const NavPoint &v)
{
    if (v.hasFlags(NAV_POINT_FLAG_BLOCKED_TEMPORARY | NAV_POINT_FLAG_BLOCKED_PERMANENT) && !v.hasFlags(NAV_POINT_FLAG_INSIDE_PIN))
        return 1.0f;
    return 2.0f;
}

void NavMesh::update(uint z)
{
    const auto &nav = mPCB.getNavGrid();
    struct Vertex { float x, y; float s; uint32_t c; };
    std::vector<Vertex> data;
    data.resize(nav.getNumPoints2D());
    assert(z < nav.getSize(2));
    const uint baseIndex = nav.LinearIndex(z, 0, 0);
    uint N = 0;
    for (uint i = baseIndex; i < (baseIndex + nav.getNumPoints2D()); ++i) {
        const auto &v = nav.getPoint(i);
        auto size = navSize(v);
        if (!size)
            continue;
        data[N].x = v.getRefPoint(&nav).x();
        data[N].y = v.getRefPoint(&nav).y();
        data[N].s = size;
        data[N].c = navColor(v, nav.getSearchSeq()).ABGR8();
        ++N;
    }
    mNumPoints = N;

    auto ctx = GL::getContext();
    if (!mVBO)
        ctx->glGenBuffers(1, &mVBO);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    ctx->glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * N, data.data(), GL_STATIC_DRAW);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, 0);
    createVAO();
}

void NavMesh::setColor(Color c)
{
    c.getFloat4(mColor4f);

    GL::getContext()->glProgramUniform4fv(sFP, sUniformColor, 1, mColor4f);
}

void NavMesh::draw(const Camera &camera)
{
    if (mColor4f[3] == 0.0f)
        return;
    auto ctx = GL::getContext();
    mPointSize = mPointSizeBase / std::sqrt(camera.getZoom());
    ctx->glBindProgramPipeline(sPP);
    ctx->glEnable(GL_PROGRAM_POINT_SIZE);
    ctx->glProgramUniform1f(sVP, sUniformPointSize, mPointSize);
    ctx->glBindVertexArray(mVAO);
    if (mColor4f[3] < 1.0f)
        ctx->glEnable(GL_BLEND);
    ctx->glDrawArrays(GL_POINTS, 0, mNumPoints);
    ctx->glDisable(GL_BLEND);
    ctx->glBindVertexArray(0);
    ctx->glDisable(GL_PROGRAM_POINT_SIZE);
    ctx->glBindProgramPipeline(0);
}

void NavMesh::createVAO()
{
    if (mVAO)
        return;
    auto ctx = GL::getContext();
    ctx->glGenVertexArrays(1, &mVAO);
    ctx->glBindVertexArray(mVAO);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    ctx->glEnableVertexAttribArray(0);
    ctx->glEnableVertexAttribArray(1);
    ctx->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 16, 0);
    ctx->glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 16, (void *)12);
    ctx->glBindVertexArray(0);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void NavMesh::createShaders()
{
    sPP = helper::loadShaderPipeline(sVP, "nav_mesh.vs.glsl", sFP, "nav_mesh.fs.glsl");

    auto ctx = GL::getContext();
    sUniformPointSize = ctx->glGetUniformLocation(sVP, "u_PointSize");
    sUniformColor = ctx->glGetUniformLocation(sFP, "u_color");
    if (sUniformPointSize < 0 ||
        sUniformColor < 0)
        throw std::runtime_error("NavMesh: Could not get location of u_PointSize or u_color.");
}
