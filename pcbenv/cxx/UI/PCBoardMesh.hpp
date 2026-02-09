
#ifndef GYM_PCB_UI_PCBOARDMESH_H
#define GYM_PCB_UI_PCBOARDMESH_H

#include <map>
#include <set>
#include "GLContext.hpp"

class AShape;
class PCBoard;
class Camera;

class PCBoardMesh
{
    constexpr static const bool AlwaysDrawComponentsAsBoxes = false;
public:
    PCBoardMesh(const PCBoard&, int z, char mask = RENDER_MASK_ALL);
    ~PCBoardMesh();

    void update();
    void draw(const Camera&);
    void drawExtra();
    void setCamera(const Camera&);
    void setExtraSet(const std::set<Object *> *);
    void setLineWidth(float w) { mLineWidthBase = w; }
    void setLineAlpha(float a) { mLineAlpha = a; }
    void setFillAlpha(float a) { mFillAlpha = a; }

    static constexpr const GLushort RestartIndex = 0xffff;

    static constexpr const char RENDER_MASK_COMPONENTS = 0x1;
    static constexpr const char RENDER_MASK_PINS       = 0x2;
    static constexpr const char RENDER_MASK_ALL        = 0x3;

    struct IndexRange {
        struct { uint16_t Line{0}, Fan{0}, Quad{0}; } Start, End;
    };

private:
    const PCBoard &mBoard;
    const int mZ;
    const char mMask;
    const std::set<Object *> *mExtraSet{0};

    GLuint mVBO[2]{0}; // Fans/Lines, Circles.
    GLuint mIBO[2]{0,0}; // Fans, Lines.
    GLuint mVAO[3]{0,0}; // Fans, Lines, Circles.
    GLsizei mIndexCountL{0};
    GLsizei mIndexCountT{0};
    GLsizei mRoundVertexCount{0};
    GLfloat mLineWidth{4.0f};
    GLfloat mLineWidthBase{4.0f};
    GLfloat mLineAlpha{1.0f};
    GLfloat mFillAlpha{1.0f};
    std::map<Object *, IndexRange> mIndexRanges;
    std::vector<GLsizei> mExtraCount;
    std::vector<GLsizei> mExtraCountRound;
    std::vector<void *> mExtraStart;
    std::vector<GLint> mExtraStartRound;

    void updateExtra();
    void countVerticesAndIndices(GLsizei &numVerts, GLsizei &numRoundVerts, GLsizei &numIndsL, GLsizei &numIndsT) const;
    void countVerticesAndIndices(GLsizei &numVerts, GLsizei &numRoundVerts, GLsizei &numIndsL, GLsizei &numIndsT, const AShape *) const;

    void *indexAddress(uint16_t) const;

private:
    static void init();
    static GLuint sVP, sRoundVP, sExtraVP;
    static GLuint sFP, sRoundFP, sExtraFP;
    static GLuint sPP, sRoundPP, sExtraPP;
    static GLint sUniformRoundAlpha;
    static GLint sUniformFillAlpha;
};

#endif // GYM_PCB_UI_PCBOARDMESH_H
