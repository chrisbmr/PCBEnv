
#include "Log.hpp"
#include "PCBoardMesh.hpp"
#include "Helper.hpp"
#include "Camera.hpp"
#include "GLContext.hpp"
#include "PCBoard.hpp"
#include "Component.hpp"
#include "Net.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

PCBoardMesh::PCBoardMesh(const PCBoard &board, int z, char mask) : mBoard(board), mZ(z), mMask(mask)
{
    init();
    update();
}
PCBoardMesh::~PCBoardMesh()
{
    auto ctx = GL::getContext();
    DEBUG("PCBoardMesh dtor(" << this << ')');
    if (mVBO[0])
        ctx->glDeleteBuffers(2, &mVBO[0]);
    if (mIBO[0])
        ctx->glDeleteBuffers(2, &mIBO[0]);
    if (mVAO[0])
        ctx->glDeleteVertexArrays(3, &mVAO[0]);
}

void PCBoardMesh::setCamera(const Camera &camera)
{
    if (camera.getZoom() > 1.75f)
        mLineWidth = 1.0f;
    else if (camera.getZoom() < 0.2f)
        mLineWidth = 4.0f;
    else mLineWidth = 2.0f;
}

template<class Vertex, class RVertex> class VertexStream
{
public:
    VertexStream(GLuint *vbo, GLuint *ibo) : mVBO(vbo), mIBO(ibo) { }
    void begin(uint vertexCount, uint roundVertexCount, uint indexCountLine, uint indexCountFan);
    void addVertex(const Point_2&, Color, Color);
    void addRoundVertex(const Point_2&, Color, Color, const Vector_2 &st, float u);
    void addLineIndexRange(uint first, uint end);
    void addFanIndexRange(uint first, uint end);
    void addShape(const Polygon_2&, Color, Color);
    void addShape(const Bbox_2&, Color, Color);
    void addShape(const Iso_rectangle_2&, Color, Color);
    void addShape(const Circle_2&, Color, Color);
    void addShape(const WideSegment_25&, Color, Color);
    void addShape(const AShape *, Color, Color);
    void endPrimitives(uint mask = 0x3);
    void end();
    uint getStride()  const {  Vertex v[2]; return (const char *)&v[1] - (const char *)&v[0]; }
    uint getStrideR() const { RVertex v[2]; return (const char *)&v[1] - (const char *)&v[0]; }
    uint getVertexPos() const { return mVertexPos; }
    uint getPrimitivePos() const { return mPrimitivePos; }
    PCBoardMesh::IndexRange endGroup();
    void beginGroup();
    std::string printVertices();
    std::string printIndices();
private:
    uint mVertexPos;
    uint mRoundVertexPos;
    uint mPrimitivePos;
    uint mLinePos;
    uint mFanPos;
    std::vector<Vertex>   mVertex;
    std::vector<RVertex>  mRoundVertex;
    std::vector<uint16_t> mIndexLine;
    std::vector<uint16_t> mIndexFan;
    GLuint *mVBO;
    GLuint *mIBO;
    PCBoardMesh::IndexRange mGroupIndexRange;
};
template<class V, class VR> void VertexStream<V,VR>::begin(uint vertexCount, uint roundVertexCount, uint indexCountLine, uint indexCountFan)
{
    mVertex.resize(vertexCount);
    mRoundVertex.resize(roundVertexCount);
    mIndexLine.resize(indexCountLine);
    mIndexFan.resize(indexCountFan);
    mVertexPos = mLinePos = mFanPos = mPrimitivePos = mRoundVertexPos = 0;
}
template<class V, class VR> void VertexStream<V,VR>::end()
{
    auto ctx = GL::getContext();
    if (!mVBO[0])
        ctx->glGenBuffers(2, mVBO);
    if (!mIBO[0])
        ctx->glGenBuffers(2, mIBO);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, mVBO[0]);
    ctx->glBufferData(GL_ARRAY_BUFFER, mVertex.size() * getStride(), mVertex.data(), GL_STATIC_DRAW);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, mVBO[1]);
    ctx->glBufferData(GL_ARRAY_BUFFER, mRoundVertex.size() * getStrideR(), mRoundVertex.data(), GL_STATIC_DRAW);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, 0);
    ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIBO[1]);
    ctx->glBufferData(GL_ELEMENT_ARRAY_BUFFER, mIndexLine.size() * sizeof(mIndexLine[0]), mIndexLine.data(), GL_STATIC_DRAW);
    ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIBO[0]);
    ctx->glBufferData(GL_ELEMENT_ARRAY_BUFFER, mIndexFan.size() * sizeof(mIndexFan[0]), mIndexFan.data(), GL_STATIC_DRAW);
    ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    mVertex.resize(0);
    mRoundVertex.resize(0);
    mIndexLine.resize(0);
    mIndexFan.resize(0);
    GL::CheckErrors("VertexStream/BufferData");
}
template<class V, class VR> void VertexStream<V,VR>::beginGroup()
{
    mGroupIndexRange.Start = mGroupIndexRange.End;
}
template<class V, class VR> PCBoardMesh::IndexRange VertexStream<V,VR>::endGroup()
{
    mGroupIndexRange.End.Line = mLinePos;
    mGroupIndexRange.End.Fan = mFanPos;
    mGroupIndexRange.End.Quad = mRoundVertexPos;
    return mGroupIndexRange;
}
template<class V, class VR> void VertexStream<V,VR>::addVertex(const Point_2 &xy, Color cL, Color cF)
{
    assert(mVertexPos < mVertex.size());
    V *v = &mVertex[mVertexPos++];
    v->x = xy.x();
    v->y = xy.y();
    v->abgr[0] = cF.ABGR8();
    v->abgr[1] = cL.ABGR8();
}
template<class V, class VR> void VertexStream<V,VR>::addRoundVertex(const Point_2 &xy, Color cL, Color cF, const Vector_2 &st, float u)
{
    assert(mRoundVertexPos < mRoundVertex.size());
    VR *v = &mRoundVertex[mRoundVertexPos++];
    v->x = xy.x();
    v->y = xy.y();
    v->s = st.x();
    v->t = st.y();
    v->u = u;
    v->abgr[0] = cF.ABGR8();
    v->abgr[1] = cL.ABGR8();
}
template<class V, class VR> void VertexStream<V,VR>::addLineIndexRange(uint first, uint end)
{
    for (uint i = first; i < end; ++i)
        mIndexLine[mLinePos++] = i;
}
template<class V, class VR> void VertexStream<V,VR>::addFanIndexRange(uint first, uint end)
{
    for (uint i = first; i < end; ++i)
        mIndexFan[mFanPos++] = i;
}
template<class V, class VR> void VertexStream<V,VR>::endPrimitives(uint mask)
{
    if (mask & 0x1)
        mIndexLine[mLinePos++] = PCBoardMesh::RestartIndex;
    if (mask & 0x2)
        mIndexFan[mFanPos++] = PCBoardMesh::RestartIndex;
    mPrimitivePos = mVertexPos;
}
template<class V, class VR> void VertexStream<V,VR>::addShape(const Polygon_2 &poly, Color cL, Color cF)
{
    for (auto I = poly.vertices_begin(); I != poly.vertices_end(); I++)
        addVertex(*I, cL, cF);
    addLineIndexRange(mPrimitivePos, mPrimitivePos + poly.size());
    if (poly.is_convex())
        addFanIndexRange(mPrimitivePos, mPrimitivePos + poly.size());
    endPrimitives(poly.is_convex() ? 0x3 : 0x1);
}
template<class V, class VR> void VertexStream<V,VR>::addShape(const Bbox_2 &box, Color cL, Color cF)
{
    addVertex(Point_2(box.xmin(), box.ymin()), cL, cF);
    addVertex(Point_2(box.xmin(), box.ymax()), cL, cF);
    addVertex(Point_2(box.xmax(), box.ymax()), cL, cF);
    addVertex(Point_2(box.xmax(), box.ymin()), cL, cF);
    addLineIndexRange(mPrimitivePos, mPrimitivePos + 4);
    addFanIndexRange(mPrimitivePos, mPrimitivePos + 4);
    endPrimitives();
}
template<class V, class VR> void VertexStream<V,VR>::addShape(const Iso_rectangle_2 &rect, Color cL, Color cF)
{
    addVertex(Point_2(rect.xmin(), rect.ymin()), cL, cF);
    addVertex(Point_2(rect.xmin(), rect.ymax()), cL, cF);
    addVertex(Point_2(rect.xmax(), rect.ymax()), cL, cF);
    addVertex(Point_2(rect.xmax(), rect.ymin()), cL, cF);
    addLineIndexRange(mPrimitivePos, mPrimitivePos + 4);
    addFanIndexRange(mPrimitivePos, mPrimitivePos + 4);
    endPrimitives();
}
template<class V, class VR> void VertexStream<V,VR>::addShape(const Circle_2 &circle, Color cL, Color cF)
{
    const auto &O = circle.center();
    const auto r = std::sqrt(circle.squared_radius());
    addRoundVertex(O + Vector_2(-r,  r), cL, cF, Vector_2(-1.0f,  1.0f), 0.0f);
    addRoundVertex(O + Vector_2( r,  r), cL, cF, Vector_2( 1.0f,  1.0f), 0.0f);
    addRoundVertex(O + Vector_2( r, -r), cL, cF, Vector_2( 1.0f, -1.0f), 0.0f);
    addRoundVertex(O + Vector_2(-r, -r), cL, cF, Vector_2(-1.0f, -1.0f), 0.0f);
}
template<class V, class VR> void VertexStream<V,VR>::addShape(const WideSegment_25 &S, Color cL, Color cF)
{
    const auto ds = S.base().getDirection(S.halfWidth());
    const auto v0 = S.source_2() - ds;
    const auto v1 = S.target_2() + ds;
    const auto dw = S.base().getPerpendicularCCW(S.halfWidth());
    const auto c = S.base().length() / (S.base().length() + S.width());
    addRoundVertex(v0 - dw, cL, cF, Vector_2(-1.0f,  1.0f), c);
    addRoundVertex(v0 + dw, cL, cF, Vector_2( 1.0f,  1.0f), c);
    addRoundVertex(v1 + dw, cL, cF, Vector_2( 1.0f, -1.0f), c);
    addRoundVertex(v1 - dw, cL, cF, Vector_2(-1.0f, -1.0f), c);
}
template<class V, class VR> void VertexStream<V,VR>::addShape(const AShape *S, Color cL, Color cF)
{
    if (auto R = S->as<IsoRectEx>()) addShape(*R, cL, cF); else
    if (auto C = S->as<CircleEx>()) addShape(*C, cL, cF); else
    if (auto Z = S->as<WideSegment_25>()) addShape(*Z, cL, cF); else
    if (auto P = S->as<PolygonEx>()) addShape(*P, cL, cF); else addShape(S->bbox(), cL, cF);
}
template<class V, class VR> std::string VertexStream<V,VR>::printVertices()
{
    std::stringstream ss;
    ss << "&0=" << &mVertex[0] << " &1=" << &mVertex[1] << " stride=" << getStride() << '\n';
    for (uint i = 0; i < mVertex.size(); ++i)
        ss << '[' << i << "] V(" << mVertex[i].x << ',' << mVertex[i].y << ") " << std_08x << mVertex[i].abgr[0] << '/' << mVertex[i].abgr[1] << '\n';
    for (uint i = 0; i < mRoundVertex.size(); ++i)
        ss << '[' << i << "] O(" << mRoundVertex[i].x << ',' << mRoundVertex[i].y << ") " << std_08x << mRoundVertex[i].abgr[0] << '/' << mRoundVertex[i].abgr[1] << '\n';
    return ss.str();
}
template<class V, class VR> std::string VertexStream<V,VR>::printIndices()
{
    std::stringstream ss;
    ss << "&0=" << &mIndexLine[0] << " &1=" << &mIndexLine[1] << '\n';
    ss << "LINEs\n";
    for (uint i = 0; i < mIndexLine.size(); ++i) {
        if (mIndexLine[i] == PCBoardMesh::RestartIndex)
            ss << '\n';
        else ss << mIndexLine[i] << ' ';
    }
    ss << "FANs\n";
    for (uint i = 0; i < mIndexFan.size(); ++i) {
        if (mIndexFan[i] == PCBoardMesh::RestartIndex)
            ss << '\n';
        else ss << mIndexFan[i] << ' ';
    }
    return ss.str();
}


void PCBoardMesh::countVerticesAndIndices(GLsizei &numVerts,
                                          GLsizei &numRoundVerts,
                                          GLsizei &numIndsL,
                                          GLsizei &numIndsT,
                                          const AShape *S) const
{
    if (S->isRound()) {
        numRoundVerts += S->vertexCount();
    } else if (S->canDrawAsTriFan()) {
        numVerts += S->vertexCount();
        numIndsL += S->vertexCount() + 1;
        numIndsT += S->indexCountWithRestart();
    } else {
        WARN("Item shape cannot be drawn as triangle fan.");
        numVerts += S->vertexCount();
        numIndsL += S->vertexCount() + 1;
        numIndsT += 0;
    }
}
void PCBoardMesh::countVerticesAndIndices(GLsizei &numVerts,
                                          GLsizei &numRoundVerts,
                                          GLsizei &numIndsL,
                                          GLsizei &numIndsT) const
{
    numVerts = 0;
    numRoundVerts = 0;
    numIndsL = 0;
    numIndsT = 0;
    if (mMask & RENDER_MASK_COMPONENTS) {
        if (AlwaysDrawComponentsAsBoxes) {
            numVerts = mBoard.getNumComponents() * 4; // Bbox
            numIndsT = mBoard.getNumComponents() * 5; // Bbox + restart
            numIndsL = mBoard.getNumComponents() * 5; // Bbox + restart
        } else {
            for (const auto C : mBoard.getComponents())
                countVerticesAndIndices(numVerts, numRoundVerts, numIndsL, numIndsT, C->getShape());
        }
    } else if (mMask & RENDER_MASK_PINS) {
        for (const auto C : mBoard.getComponents())
            for (const auto P : C->getChildren())
                countVerticesAndIndices(numVerts, numRoundVerts, numIndsL, numIndsT, P->getShape());
    }
}

void PCBoardMesh::update()
{
    struct Vertex { float x, y; uint32_t abgr[2]; };
    struct RoundVertex { float x, y; uint32_t abgr[2]; float s, t, u; };
    VertexStream<Vertex, RoundVertex> stream(mVBO, mIBO);
    GLsizei numVertices;
    countVerticesAndIndices(numVertices, mRoundVertexCount, mIndexCountL, mIndexCountT);
    stream.begin(numVertices, mRoundVertexCount, mIndexCountL, mIndexCountT);

    for (auto C : mBoard.getComponents()) {
        if (C->getSingleLayer() != mZ)
            continue;
        if (mMask & RENDER_MASK_COMPONENTS) {
            stream.beginGroup();
            if (AlwaysDrawComponentsAsBoxes)
                stream.addShape(C->getBbox(), C->getLineColor(), C->getFillColor());
            else
                stream.addShape(C->getShape(), C->getLineColor(), C->getFillColor());
            mIndexRanges[C] = stream.endGroup();
        }
        if (!(mMask & RENDER_MASK_PINS))
            continue;
        // Write pins
        for (const auto P : C->getChildren()) {
            stream.beginGroup();
            const auto S = P->getShape();
            const auto P_ = P->as<Pin>();
            stream.addShape(S, P_->colorLine(), P_->colorFill());
            mIndexRanges[P] = stream.endGroup();
        }
    }
#if 0
    stream.printVertices();
    stream.printIndices();
#endif
    stream.end();

    updateExtra();

    if (mVAO[0])
        return;
    auto ctx = GL::getContext();
    ctx->glGenVertexArrays(3, mVAO);
    for (uint i = 0; i < 3; ++i) {
        const auto stride = (i == 2) ? stream.getStrideR() : stream.getStride();
        ctx->glBindVertexArray(mVAO[i]);
        ctx->glBindBuffer(GL_ARRAY_BUFFER, mVBO[i == 2 ? 1 : 0]);
        if (i != 2)
            ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIBO[i]);
        ctx->glEnableVertexAttribArray(0);
        ctx->glEnableVertexAttribArray(1);
        if (i == 2) {
            ctx->glEnableVertexAttribArray(2);
            ctx->glEnableVertexAttribArray(3);
        }
        ctx->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, 0);
        ctx->glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void *)(i == 1 ? 12ULL : 8ULL));
        if (i == 2) {
            ctx->glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void *)12ULL);
            ctx->glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void *)16ULL);
        }
        GL::CheckErrors("PCBoardMesh/VAO");
    }
    ctx->glBindVertexArray(0);
    ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, 0);
}

inline void *PCBoardMesh::indexAddress(uint16_t i) const
{
    return reinterpret_cast<void *>(std::intptr_t(i) * 2);
}
void PCBoardMesh::updateExtra()
{
    mExtraStart.clear();
    mExtraCount.clear();
    mExtraStartRound.clear();
    mExtraCountRound.clear();
    if (!mExtraSet)
        return;
    for (auto I : *mExtraSet) {
        if (!I->isOnLayer(mZ))
            continue;
        auto R = mIndexRanges.find(I);
        if (R == mIndexRanges.end())
            continue;
        const uint NF = R->second.End.Fan - R->second.Start.Fan;
        if (NF >= 3) {
            mExtraStart.push_back(indexAddress(R->second.Start.Fan));
            mExtraCount.push_back(NF - 1); // -1 as last restart index not needed
        }
        const uint NQ = R->second.End.Quad - R->second.Start.Quad;
        if (NQ >= 4) {
            mExtraStartRound.push_back(R->second.Start.Quad);
            mExtraCountRound.push_back(NQ);
        }
    }
}
void PCBoardMesh::setExtraSet(const std::set<Object *> *list)
{
    mExtraSet = list;
    updateExtra();
}

void PCBoardMesh::drawExtra()
{
    if (mExtraCount.empty() && mExtraCountRound.empty())
        return;
    auto ctx = GL::getContext();
    ctx->glEnable(GL_BLEND);
    ctx->glBlendFunc(GL_ONE, GL_ONE);
    if (mExtraCount.size()) {
        ctx->glBindProgramPipeline(sExtraPP);
        ctx->glBindVertexArray(mVAO[0]);
        ctx->glProgramUniform1f(sFP, sUniformFillAlpha, 0.5f);
        ctx->glEnable(GL_PRIMITIVE_RESTART);
        ctx->glMultiDrawElements(GL_TRIANGLE_FAN, &mExtraCount[0], GL_UNSIGNED_SHORT, &mExtraStart[0], mExtraCount.size());
        ctx->glDisable(GL_PRIMITIVE_RESTART);
    }
    if (mExtraCountRound.size()) {
        ctx->glBindProgramPipeline(sExtraPP);
        ctx->glBindVertexArray(mVAO[2]);
        ctx->glProgramUniform1f(sFP, sUniformFillAlpha, 0.5f);
        ctx->glMultiDrawArrays(GL_QUADS, &mExtraStartRound[0], &mExtraCountRound[0], mExtraCountRound.size());
    }
    ctx->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    ctx->glDisable(GL_BLEND);
    ctx->glBindVertexArray(0);
    ctx->glBindProgramPipeline(0);
}

void PCBoardMesh::draw(const Camera &camera)
{
    auto ctx = GL::getContext();
    GL::CheckErrors("PCBoardMesh/PreDraw");

    setCamera(camera);

    ctx->glEnable(GL_PRIMITIVE_RESTART);
    ctx->glBindProgramPipeline(sPP);
    if (GL::CheckErrors("PCBoardMesh/Program"))
        init();
    if (mFillAlpha > 0.0f) {
        if (mFillAlpha < 1.0f)
            ctx->glEnable(GL_BLEND);
        ctx->glBindVertexArray(mVAO[0]);
        ctx->glProgramUniform1f(sFP, sUniformFillAlpha, mFillAlpha);
        ctx->glDrawElements(GL_TRIANGLE_FAN, mIndexCountT, GL_UNSIGNED_SHORT, 0);
        if (mFillAlpha < 1.0f)
            ctx->glDisable(GL_BLEND);
    }
    if (mLineWidth > 0.0f && mLineAlpha > 0.0f) {
        if (mLineWidth != 1.0f)
            ctx->glLineWidth(mLineWidth);
        if (mLineAlpha < 1.0f)
            ctx->glEnable(GL_BLEND);
        ctx->glBindVertexArray(mVAO[1]);
        ctx->glProgramUniform1f(sFP, sUniformFillAlpha, mLineAlpha);
        ctx->glDrawElements(GL_LINE_LOOP, mIndexCountL, GL_UNSIGNED_SHORT, 0);
        if (mLineAlpha < 1.0f)
            ctx->glDisable(GL_BLEND);
        if (mLineWidth != 1.0f)
            ctx->glLineWidth(1.0f);
    }
    ctx->glDisable(GL_PRIMITIVE_RESTART);
    if (mRoundVertexCount && (mFillAlpha > 0.0f || mLineAlpha > 0.0f)) {
        ctx->glEnable(GL_BLEND);
        ctx->glBindProgramPipeline(sRoundPP);
        ctx->glProgramUniform2f(sRoundFP, sUniformRoundAlpha, mFillAlpha, mLineAlpha);
        ctx->glBindVertexArray(mVAO[2]);
        ctx->glDrawArrays(GL_QUADS, 0, mRoundVertexCount);
        ctx->glDisable(GL_BLEND);
    }
    GL::CheckErrors("PCBoardMesh/AfterDraw");
    ctx->glBindVertexArray(0);
    ctx->glBindProgramPipeline(0);
}



GLuint PCBoardMesh::sVP = 0, PCBoardMesh::sRoundVP = 0, PCBoardMesh::sExtraVP = 0;
GLuint PCBoardMesh::sFP = 0, PCBoardMesh::sRoundFP = 0, PCBoardMesh::sExtraFP = 0;
GLuint PCBoardMesh::sPP = 0, PCBoardMesh::sRoundPP = 0, PCBoardMesh::sExtraPP = 0;

GLint PCBoardMesh::sUniformFillAlpha = -1;
GLint PCBoardMesh::sUniformRoundAlpha = -1;

void PCBoardMesh::init()
{
    auto ctx = GL::getContext();

    DEBUG("PCBoardMesh: building shaders");

    sPP      = helper::loadShaderPipeline(sVP, "board_mesh.vs.glsl", sFP, "board_mesh.fs.glsl");
    sRoundPP = helper::loadShaderPipeline(sRoundVP, "round_mesh.vs.glsl", sRoundFP, "round_mesh.fs.glsl");
    sExtraPP = helper::loadShaderPipeline(sExtraVP, "basic.vs.glsl", sExtraFP, "hilight_mesh.fs.glsl");

    sUniformFillAlpha = ctx->glGetUniformLocation(sFP, "u_FillAlpha");
    sUniformRoundAlpha = ctx->glGetUniformLocation(sRoundFP, "u_FillLineAlpha");
    if (sUniformFillAlpha < 0 ||
        sUniformRoundAlpha < 0)
        throw std::runtime_error("PCBoard: Could not get location of u_FillAlpha.");
}
