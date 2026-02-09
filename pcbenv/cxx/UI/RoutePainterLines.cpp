
#include "RoutePainterLines.hpp"
#include "Camera.hpp"
#include "GLContext.hpp"
#include "Helper.hpp"
#include "PCBoard.hpp"
#include "Component.hpp"
#include "Net.hpp"
#include "Track.hpp"

GLuint RoutePainterLines::sVP = 0;
GLuint RoutePainterLines::sFP = 0;
GLuint RoutePainterLines::sPP = 0;

GLint RoutePainterLines::sUniformColors = -1;

RoutePainterLines::RoutePainterLines(const PCBoard &pcb) : mPCB(pcb)
{
    for (uint c = 0; c < 4; ++c) {
        mColorVars[0][c] = 1.0f;
        mColorVars[1][c] = 0.0f;
    }
    createShaders();
    update(std::vector<Connection *>());
}

RoutePainterLines::~RoutePainterLines()
{
    auto ctx = GL::getContext();
    if (mVBO)
        ctx->glDeleteBuffers(1, &mVBO);
    if (mCMD)
        ctx->glDeleteBuffers(1, &mCMD);
    if (mVAO)
        ctx->glDeleteVertexArrays(1, &mVAO);
}

namespace {

struct Vertex
{
    GLfloat x, y, z; uint32_t c; void write(const Point_2 &V, float Z, uint32_t C) { x = V.x(); y = V.y(); z = Z; c = C; }
};
class VertexBuffer
{
public:
    VertexBuffer(uint reserve);
    const void *data() const { return vec.data(); }
    std::size_t count() const { return vec.size(); }
    std::size_t size() const { return vec.size() * sizeof(Vertex); }
    void addTrackAsLines(const Track&);
    void addTrackAsLineStrip(const Track&);
private:
    Vertex *moreVertices(uint count) { auto n = vec.size(); vec.resize(n + count); return &vec[n]; }
private:
    std::vector<Vertex> vec;
};
VertexBuffer::VertexBuffer(uint reserve)
{
    vec.reserve(reserve);
}
void VertexBuffer::addTrackAsLineStrip(const Track &route)
{
    auto c32 = Color::RED.ABGR8(); // route.getNet()->getColor().ABGR8();
    auto V = moreVertices(route.numSegments() + 1);
    uint i = 0;
    V[i++].write(route.start().xy(), route.start().z(), c32);
    for (const auto &S : route.getSegments())
        V[i++].write(S.target_2(), S.z(), c32); // FIXME: Which z-value is used?
}
void VertexBuffer::addTrackAsLines(const Track &route)
{
    auto c32 = Color::RED.ABGR8(); // route.getNet()->getColor().ABGR8();
    auto V = moreVertices(route.numSegments() * 2);
    uint i = 0;
    for (const auto &S : route.getSegments()) {
        V[i++].write(S.source_2(), S.z(), c32); // FIXME: Which z-value is used?
        V[i++].write(S.target_2(), S.z(), c32); // FIXME: Which z-value is used?
    }
}

struct DrawCommand
{
    DrawCommand(uint start, uint count) : count(count), instanceCount(1), first(start), baseInstance(0) { }
    GLuint count;
    GLuint instanceCount;
    GLuint first;
    GLuint baseInstance;
};
class CommandBuffer
{
public:
    CommandBuffer(uint reserve) { vec.reserve(reserve); }
    const void *data() const { return vec.data(); }
    std::size_t count() const { return vec.size(); }
    std::size_t size() const { return vec.size() * sizeof(DrawCommand); }
    void addCommand(uint start, uint count) { vec.emplace_back(DrawCommand(start, count)); }
private:
    std::vector<DrawCommand> vec;
};

} // anon namespace

void RoutePainterLines::update(const std::vector<Connection *> &routes)
{
    VertexBuffer buf(mVertexCountEstimate);
    CommandBuffer cmd(mNumLineStrips);
    for (const Connection *X : routes) {
        if (!X->hasTracks())
            continue;
        const uint start = buf.count();
        for (const auto *T : X->getTracks()) {
            if (UseLineStrips)
                buf.addTrackAsLineStrip(*T);
            else
                buf.addTrackAsLines(*T);
        }
        cmd.addCommand(start, buf.count() - start);
    }
    mNumLineStrips = cmd.count();
    mVertexCountEstimate = buf.count();

    auto ctx = GL::getContext();
    if (!mVBO) {
        ctx->glGenBuffers(1, &mVBO);
        ctx->glGenBuffers(1, &mCMD);
    }
    ctx->glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    ctx->glBufferData(GL_ARRAY_BUFFER, buf.size(), buf.data(), GL_STATIC_DRAW);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, 0);
    ctx->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, mCMD);
    ctx->glBufferData(GL_DRAW_INDIRECT_BUFFER, cmd.size(), cmd.data(), GL_STATIC_DRAW);
    ctx->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    createVAO();
}

void RoutePainterLines::draw(const Camera &camera)
{
    const auto Primitive = UseLineStrips ? GL_LINE_STRIP : GL_LINES;
    auto ctx = GL::getContext();
    auto lineWidth = camera.adjustLineWidth(mBaseLineWidth);
    ctx->glBindProgramPipeline(sPP);
    ctx->glProgramUniform4fv(sFP, sUniformColors, 2, &mColorVars[0][0]);
    if (lineWidth != 1.0f)
        ctx->glLineWidth(lineWidth);
    ctx->glBindVertexArray(mVAO);
    ctx->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, mCMD);
    ctx->glMultiDrawArraysIndirect(Primitive, 0, mNumLineStrips, sizeof(DrawCommand));
    ctx->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    ctx->glBindVertexArray(0);
    if (lineWidth != 1.0f)
        ctx->glLineWidth(1.0f);
    ctx->glBindProgramPipeline(0);
}

void RoutePainterLines::createVAO()
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
    ctx->glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 16, (void *)12ULL);
    ctx->glBindVertexArray(0);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RoutePainterLines::createShaders()
{
    sPP = helper::loadShaderPipeline(sVP, "basic_color-flat.vs.glsl", sFP, "rats_nest.fs.glsl");

    sUniformColors = GL::getContext()->glGetUniformLocation(sFP, "u_colors");
    if (sUniformColors < 0)
        throw std::runtime_error("RouterPainterLines: Could not get location of u_colors.");
}
