
#include "RatsNest.hpp"
#include "Camera.hpp"
#include "GLContext.hpp"
#include "Helper.hpp"
#include "PCBoard.hpp"
#include "Component.hpp"
#include "Net.hpp"

GLuint RatsNest::sVP = 0;
GLuint RatsNest::sFP = 0;
GLuint RatsNest::sPP = 0;

GLint RatsNest::sUniformColors = -1;

RatsNest::RatsNest(const PCBoard &pcb) : mPCB(pcb)
{
    mLayerAlpha.resize(pcb.getNumLayers(), 1.0f);
    createShaders();
    update();
    setColorHL(Color::RED);
}

RatsNest::~RatsNest()
{
    auto ctx = GL::getContext();
    if (mVBO)
        ctx->glDeleteBuffers(1, &mVBO);
    if (mVAO)
        ctx->glDeleteVertexArrays(1, &mVAO);
}

void RatsNest::setColorHL(Color c)
{
    c.getFloat4(mColorVars[1][1]);
    for (uint c = 0; c < 4; ++c) {
        mColorVars[0][0][c] = 1.0f;
        mColorVars[0][1][c] = 0.0f;
        mColorVars[1][0][c] = 0.0f;
    }
    GL::getContext()->glProgramUniform4fv(sFP, sUniformColors, 2, &mColorVars[0][0][0]);
}

void RatsNest::setHL(const std::vector<Connection *> *vec)
{
    mHLConns.clear();
    if (vec)
        mHLConns.insert(mHLConns.begin(), vec->begin(), vec->end());
    updateHL();
}
void RatsNest::updateHL()
{
    mIndicesHL.clear();
    for (const auto *X : mHLConns) {
        auto I = mConn2Index.find(X);
        if (I != mConn2Index.end())
            for (auto i = I->second.first; i != I->second.second; ++i)
                mIndicesHL.push_back(i);
    }
}

inline float RatsNest::getLayerAlpha(const Pin *T) const
{
    if (!T)
        return 1.0f;
    // auto a = getLayerAlpha(T->getLayerMin());
    // for (auto z = T->getLayerMin() + 1; z <= T->getLayerMax(); ++z)
    //     a = std::max(a, getLayerAlpha(z));
    return getLayerAlpha(T->getParent()->getSingleLayer());
}
bool RatsNest::shouldAdd(const Connection &X) const
{
    if (X.isRouted())
        return false;
    if (getLayerAlpha(X.sourcePin()) <= 0.0f && getLayerAlpha(X.targetPin()) <= 0.0f)
        return false;
    if (allVisible())
        return true;
    return
        (X.targetPin() && (X.targetPin()->isSelected() || X.targetPin()->getParent()->isSelected())) ||
        (X.sourcePin() && (X.sourcePin()->isSelected() || X.sourcePin()->getParent()->isSelected()));
}

namespace {

struct Vertex { float x, y; uint32_t color; };

inline void writePath(std::vector<Vertex> &buf, const Connection &X, float a0, float a1)
{
    Vertex v[2];
    v[0].color = X.getColor().withAlpha(a0).ABGR8();
    v[1].color = X.getColor().withAlpha(a1).ABGR8();
    for (const auto &rat : X.getRatsNest()) {
        v[0].x = rat.first.x();
        v[0].y = rat.first.y();
        v[1].x = rat.second.x();
        v[1].y = rat.second.y();
        buf.push_back(v[0]);
        buf.push_back(v[1]);
    }
}

} // anon namespace


void RatsNest::update()
{
    auto ctx = GL::getContext();

    mConn2Index.clear();
    std::vector<Vertex> buf;
    for (auto net : mPCB.getNets()) {
        if (net->getColor().isVisible()) {
            for (const Connection *X : net->connections()) {
                assert(buf.size() <= 0xffff);
                if (!shouldAdd(*X))
                    continue;
                const auto first = uint16_t(buf.size());
                writePath(buf, *X, getLayerAlpha(X->sourcePin()), getLayerAlpha(X->targetPin()));
                mConn2Index[X] = std::make_pair(first, uint16_t(buf.size()));
            }
        }
    }
    mNumLines = buf.size();
    if (!mVBO)
        ctx->glGenBuffers(1, &mVBO);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    ctx->glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * buf.size(), buf.data(), GL_STATIC_DRAW);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, 0);
    createVAO();

    updateHL();
}

void RatsNest::draw(const Camera &camera)
{
    auto ctx = GL::getContext();
    bool setLW = mLineWidth != 1.0f;
    ctx->glBindProgramPipeline(sPP);
    ctx->glBindVertexArray(mVAO);
    ctx->glProgramUniform4fv(sFP, sUniformColors, 2, &mColorVars[0][0][0]);
    if (setLW)
        ctx->glLineWidth(mLineWidth);
    ctx->glEnable(GL_BLEND);
    ctx->glDrawArrays(GL_LINES, 0, mNumLines);
    ctx->glDisable(GL_BLEND);
    if (mIndicesHL.size()) {
        setLW = true;
        ctx->glProgramUniform4fv(sFP, sUniformColors, 2, &mColorVars[1][0][0]);
        ctx->glLineWidth(mLineWidth * 2.0f);
        ctx->glDrawElements(GL_LINES, mIndicesHL.size(), GL_UNSIGNED_SHORT, mIndicesHL.data());
    }
    ctx->glBindVertexArray(0);
    ctx->glBindProgramPipeline(0);
    if (setLW)
        ctx->glLineWidth(1.0f);
}

void RatsNest::createVAO()
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
    ctx->glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 12, (void *)8ULL);
    ctx->glBindVertexArray(0);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RatsNest::createShaders()
{
    sPP = helper::loadShaderPipeline(sVP, "basic_color.vs.glsl", sFP, "rats_nest.fs.glsl");

    sUniformColors = GL::getContext()->glGetUniformLocation(sFP, "u_colors");
    if (sUniformColors < 0)
        throw std::runtime_error("RatsNest: Could not get location of u_colors.");
}
