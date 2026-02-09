
#include "Log.hpp"
#include "RoutePainter.hpp"
#include "Camera.hpp"
#include "GLContext.hpp"
#include "Helper.hpp"
#include "PCBoard.hpp"
#include "Pin.hpp"
#include "Connection.hpp"
#include "Net.hpp"
#include "Track.hpp"

GLuint RoutePainter::sVP  = 0;
GLuint RoutePainter::sFP  = 0;
GLuint RoutePainter::sPP  = 0;

GLint RoutePainter::sUniformLayerColors = -1;

RoutePainter::RoutePainter()
{
    createShaders();
    for (uint z = 0; z < 16; ++z)
        setLayerColor(z, Palette::Tracks[z]);
}

RoutePainter::~RoutePainter()
{
    auto ctx = GL::getContext();
    if (mVBO)
        ctx->glDeleteBuffers(1, &mVBO);
    if (mIBO)
        ctx->glDeleteBuffers(1, &mIBO);
    if (mCMD)
        ctx->glDeleteBuffers(1, &mCMD);
    if (mVAO)
        ctx->glDeleteVertexArrays(1, &mVAO);
}

struct Vertex
{
    GLfloat x, y, z, s; uint32_t c; float r; void write(const Point_2 &V, float Z, float S, uint32_t C, float R = 0.0f) { x = V.x(); y = V.y(); z = Z; s = S; c = C; r = R; }
};
class VertexBuffer
{
public:
    VertexBuffer(uint &vbo, uint numZ);
    uint count() const { return mCountSum; }
    std::vector<uint> counts() const { return mCounts; }
    void countTrack(const Track&);
    void writeTrack(const Track&, const Color&);
    void countNet(const Net&);
    void writeNet(const Net&);
    void startVBO();
    void endVBO();
private:
    Vertex *moreVertices(uint count, uint z) { auto v = mBufferPtr[z]; mBufferPtr[z] += count; return v; }
    void addQuad(const WideSegment_25&);
private:
    std::vector<uint> mCounts;
    std::vector<Vertex *> mBufferPtr;
    uint mCountSum;
    GLuint &mVBO;
    uint32_t c32;
};
VertexBuffer::VertexBuffer(uint &vbo, uint numZ) : mVBO(vbo), mCounts(numZ, 0), mBufferPtr(numZ)
{
}
void VertexBuffer::addQuad(const WideSegment_25 &S)
{
    assert(uint(S.z()) < mBufferPtr.size());
    const auto v0 = S.source_2();
    const auto v1 = S.target_2();
    const auto ds = S.base().getDirection(S.halfWidth());
    const auto dw = S.base().getPerpendicularCCW(S.halfWidth());
    auto V = moreVertices(8, S.z());
    V[0].write(v0 - dw - ds, S.z(), -1.0f, c32, -1.0f); // OBL
    V[1].write(v0 + dw - ds, S.z(),  1.0f, c32, -1.0f); // OTL
    V[2].write(v0 - dw, S.z(), -1.0f, c32);             // IBL
    V[3].write(v0 + dw, S.z(),  1.0f, c32);             // ITL
    V[4].write(v1 - dw, S.z(), -1.0f, c32);             // IBR
    V[5].write(v1 + dw, S.z(),  1.0f, c32);             // ITR
    V[6].write(v1 - dw + ds, S.z(), -1.0f, c32, 1.0f);  // OBR
    V[7].write(v1 + dw + ds, S.z(),  1.0f, c32, 1.0f);  // OTR
}
void VertexBuffer::countTrack(const Track &T)
{
    for (const auto &S : T.getSegments())
        mCounts.at(S.z()) += 8;
}
void VertexBuffer::countNet(const Net &net)
{
    if (net.getColor().isVisible())
        for (const auto X : net.connections())
            for (const auto *T : X->getTracks())
                countTrack(*T);
}
void VertexBuffer::writeTrack(const Track &T, const Color &c)
{
    c32 = c.ABGR8();
    for (const auto &s : T.getSegments())
        addQuad(s);
}
void VertexBuffer::writeNet(const Net &net)
{
    if (net.getColor().isVisible())
        for (const auto X : net.connections())
            for (const auto *T : X->getTracks())
                writeTrack(*T, X->getColor());
}

void VertexBuffer::startVBO()
{
    mCountSum = mCounts[0];
    for (uint i = 1; i < mCounts.size(); ++i)
        mCountSum += mCounts[i];
    auto ctx = GL::getContext();
    if (!mVBO)
        ctx->glGenBuffers(1, &mVBO);
    GLint size = mCountSum * sizeof(Vertex);
    GLint vboSize;
    ctx->glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    ctx->glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &vboSize);
    if (size > vboSize || size < (vboSize - 4096))
        ctx->glBufferData(GL_ARRAY_BUFFER, size, 0, GL_STATIC_DRAW);
    if (mCountSum == 0)
        return;
    Vertex *dma = (Vertex *)ctx->glMapBufferRange(GL_ARRAY_BUFFER, 0, size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    for (int i = mCounts.size() - 1; i >= 0; dma += mCounts[i], --i)
        mBufferPtr[i] = dma;
}
void VertexBuffer::endVBO()
{
    if (mCountSum == 0)
        return;
    auto ctx = GL::getContext();
    ctx->glUnmapBuffer(GL_ARRAY_BUFFER);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RoutePainter::update(const PCBoard &PCB)
{
    VertexBuffer buf(mVBO, PCB.getNumLayers());
    for (auto net : PCB.getNets())
        buf.countNet(*net);
    buf.startVBO();
    for (auto net : PCB.getNets())
        buf.writeNet(*net);
    buf.endVBO();
    mVertexCount = buf.count();
    mIndexCounts = buf.counts();
    for (auto &n : mIndexCounts)
        n = (n / 8) * 9;
    createIBO();
    createVAO();
}
void RoutePainter::update(const Track &T, uint zmax, Color c)
{
    VertexBuffer buf(mVBO, zmax + 1);
    buf.countTrack(T);
    buf.startVBO();
    buf.writeTrack(T, c);
    buf.endVBO();
    mVertexCount = buf.count();
    mIndexCounts = buf.counts();
    for (auto &n : mIndexCounts)
        n = (n / 8) * 9;
    createIBO();
    createVAO();
}
void RoutePainter::clear()
{
    mVertexCount = 0;
    mIndexCounts.clear();
}

template<typename T> void WriteIndices(GLWidget *ctx, uint vertexCount, GLint size)
{
    T *I = new T[9 * (vertexCount / 8)];
    if (!I)
        return;
    uint m = 0;
    for (T n = 0; n < vertexCount; n += 8) {
        for (T i = n; i < (n + 8); ++i)
            I[m++] = i;
        I[m++] = (T)~0u;
    }
    ctx->glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, I, GL_STATIC_DRAW);
    delete[] I;
}
void RoutePainter::createIBO()
{
    assert((mVertexCount % 8) == 0);
    const uint indexCount = (mVertexCount / 8) * 9;
    mIndexType = (mVertexCount > 65535) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
    const GLint minSize = indexCount * (mIndexType == GL_UNSIGNED_INT ? sizeof(GLuint) : sizeof(GLushort));
    auto ctx = GL::getContext();
    if (!mIBO)
        ctx->glGenBuffers(1, &mIBO);
    ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIBO);
    GLint curSize;
    ctx->glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &curSize);
    if (curSize < minSize || (curSize - minSize) > 4096)
        (mIndexType == GL_UNSIGNED_INT) ? WriteIndices<GLuint>(ctx, mVertexCount, minSize) : WriteIndices<GLushort>(ctx, mVertexCount, minSize);
    ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void RoutePainter::draw(const Camera &camera)
{
    auto ctx = GL::getContext();
    ctx->glEnable(GL_STENCIL_TEST);
    ctx->glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
    ctx->glEnable(GL_BLEND);
    ctx->glBindProgramPipeline(sPP);
    ctx->glBindVertexArray(mVAO);
    ctx->glEnable(GL_PRIMITIVE_RESTART);
    for (int z = mIndexCounts.size() - 1, start = 0, s = 1; z >= 0; s += 1, start += mIndexCounts[z--]) {
        if (!mIndexCounts[z])
            continue;
        const std::uintptr_t base = (mIndexType == GL_UNSIGNED_INT) ? (start * 4) : (start * 2);
        ctx->glStencilFunc(GL_GREATER, s, 0xff);
        ctx->glDrawElementsInstanced(GL_TRIANGLE_STRIP, mIndexCounts[z] - 1, mIndexType, (void *)base, 2);
    }
    ctx->glDisable(GL_PRIMITIVE_RESTART);
    ctx->glBindVertexArray(0);
    ctx->glDisable(GL_BLEND);
    ctx->glDisable(GL_STENCIL_TEST);
    ctx->glBindProgramPipeline(0);
}
void RoutePainter::drawLayer(const Camera &camera, uint z)
{
    auto ctx = GL::getContext();
    if (z >= mIndexCounts.size() || !mIndexCounts[z])
        return;
    uint start = 0;
    for (uint i = mIndexCounts.size() - 1; i > z; --i)
        start += mIndexCounts[i];
    const std::uintptr_t base = (mIndexType == GL_UNSIGNED_INT) ? (start * 4) : (start * 2);
    ctx->glBindProgramPipeline(sPP);
    ctx->glBindVertexArray(mVAO);
    ctx->glEnable(GL_PRIMITIVE_RESTART);
    ctx->glDrawElementsInstancedBaseInstance(GL_TRIANGLE_STRIP, mIndexCounts[z] - 1, mIndexType, (void *)base, 1, 2);
    ctx->glDisable(GL_PRIMITIVE_RESTART);
    ctx->glBindVertexArray(0);
    ctx->glBindProgramPipeline(0);
}

void RoutePainter::createVAO()
{
    if (mVAO)
        return;
    auto ctx = GL::getContext();
    ctx->glGenVertexArrays(1, &mVAO);
    ctx->glBindVertexArray(mVAO);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIBO);
    ctx->glEnableVertexAttribArray(0);
    ctx->glEnableVertexAttribArray(1);
    ctx->glEnableVertexAttribArray(2);
    ctx->glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 24, 0);
    ctx->glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 24, (void *)16ULL);
    ctx->glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 24, (void *)20ULL);
    ctx->glBindVertexArray(0);
    ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RoutePainter::setLayerColor(uint z, Color c)
{
    assert(z < MaxLayers);
    if (z >= MaxLayers)
        return;
    c.getFloat4(mLayerColor[z]);
    GL::getContext()->glProgramUniform4fv(sVP, sUniformLayerColors + z, 1, mLayerColor[z]);
}
void RoutePainter::setLayerAlpha(uint z, float a)
{
    assert(z < MaxLayers);
    if (z >= MaxLayers)
        return;
    mLayerColor[z][3] = a;
    GL::getContext()->glProgramUniform4fv(sVP, sUniformLayerColors + z, 1, mLayerColor[z]);
}

void RoutePainter::createShaders()
{
    sPP  = helper::loadShaderPipeline(sVP, "routes-pretty.vs.glsl", sFP,  "routes-pretty.fs.glsl");
    sUniformLayerColors = GL::getContext()->glGetUniformLocation(sVP, "u_LayerColor");
    if (sUniformLayerColors < 0)
        throw std::runtime_error("RoutePainter: Could not get location of u_LayerColor.");
}
