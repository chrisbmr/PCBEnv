
#include "ViaPainter.hpp"
#include "Camera.hpp"
#include "GLContext.hpp"
#include "Helper.hpp"
#include "PCBoard.hpp"
#include "Net.hpp"
#include "Track.hpp"

GLuint ViaPainter::sVP = 0;
GLuint ViaPainter::sFP = 0;
GLuint ViaPainter::sPP = 0;

ViaPainter::ViaPainter()
{
    createShaders();
}

ViaPainter::~ViaPainter()
{
    auto ctx = GL::getContext();
    if (mVBO)
        ctx->glDeleteBuffers(1, &mVBO);
    if (mVAO)
        ctx->glDeleteVertexArrays(1, &mVAO);
}

struct Vertex
{
    GLfloat x, y, s, t; uint32_t c1; uint32_t c2; void write(const Point_2 &V, float S, float T, uint32_t C1, uint32_t C2) { x = V.x(); y = V.y(); s = S; t = T; c1 = C1; c2 = C2; }
};

void ViaPainter::update(const PCBoard &PCB)
{
    std::vector<const Via *> vias;
    for (const auto net : PCB.getNets())
        for (const auto X : net->connections())
            for (const auto *T : X->getTracks())
                for (const auto &via : T->getVias())
                    vias.push_back(&via);
    update(vias);
}
void ViaPainter::update(const Track &T)
{
    std::vector<const Via *> vias;
    for (const auto &via : T.getVias())
        vias.push_back(&via);
    update(vias);
}
void ViaPainter::update(const std::vector<const Via *> &vias)
{
    mNumVertices = vias.size() * 4;
    if (!mNumVertices)
        return;
    const GLuint size = mNumVertices * sizeof(Vertex);
    auto ctx = GL::getContext();
    if (!mVBO)
        ctx->glGenBuffers(1, &mVBO);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    ctx->glBufferData(GL_ARRAY_BUFFER, size, 0, GL_STATIC_DRAW);
    Vertex *V = (Vertex *)ctx->glMapBufferRange(GL_ARRAY_BUFFER, 0, size, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
    for (auto via : vias) {
        const auto c1 = Palette::Tracks[via->zmax()].ABGR8();
        const auto c2 = Palette::Tracks[via->zmin()].ABGR8();
        const auto r = via->radius();
        const auto v = via->location();
        V[0].write(v + Vector_2(-r, -r), -1.0f, -1.0f, c1, c2);
        V[1].write(v + Vector_2(-r,  r), -1.0f,  1.0f, c1, c2);
        V[2].write(v + Vector_2( r,  r),  1.0f,  1.0f, c1, c2);
        V[3].write(v + Vector_2( r, -r),  1.0f, -1.0f, c1, c2);
        V += 4;
    }
    ctx->glUnmapBuffer(GL_ARRAY_BUFFER);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, 0);

    createVAO();
}

void ViaPainter::draw(const Camera &camera)
{
    if (!mNumVertices)
        return;
    auto ctx = GL::getContext();
    ctx->glBindProgramPipeline(sPP);
    ctx->glBindVertexArray(mVAO);
    ctx->glEnable(GL_BLEND);
    ctx->glDrawArrays(GL_QUADS, 0, mNumVertices);
    ctx->glDisable(GL_BLEND);
    ctx->glBindVertexArray(0);
    ctx->glBindProgramPipeline(0);
}

void ViaPainter::createVAO()
{
    if (mVAO)
        return;
    auto ctx = GL::getContext();
    ctx->glGenVertexArrays(1, &mVAO);
    ctx->glBindVertexArray(mVAO);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    ctx->glEnableVertexAttribArray(0);
    ctx->glEnableVertexAttribArray(1);
    ctx->glEnableVertexAttribArray(2);
    ctx->glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 24, 0);
    ctx->glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 24, (void *)16ULL);
    ctx->glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, 24, (void *)20ULL);
    ctx->glBindVertexArray(0);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ViaPainter::createShaders()
{
    sPP = helper::loadShaderPipeline(sVP, "via.vs.glsl", sFP, "via.fs.glsl");
}
