
#ifndef GYM_PCB_UI_GLWIDGET_H
#define GYM_PCB_UI_GLWIDGET_H

#include "Py.hpp"
#include <QOpenGLWidget>
#include <QOpenGLFunctions_4_3_Core>
#include <QPainter>
#include <QElapsedTimer>
#include "Camera.hpp"
#include "Shaders.hpp"
#include <set>

class Object;
class Component;
class Pin;
class Net;
class PCBoard;
class PCBoardMesh;
class NavMesh;
namespace ui {
class NavGrid;
} // namespace ui
class RatsNest;
class RoutePainter;
class RoutePainterLines;
class TriList;
class ViaPainter;
class UIApplication;
class UIActions;
class Window;

class GLWidget : public QOpenGLWidget, public QOpenGLFunctions_4_3_Core
{
    Q_OBJECT

private:
    GLWidget(Window *parent, UIActions *A);
    ~GLWidget();
public:
    static GLWidget *createInstance(Window *, UIActions *);
    static void deleteInstance();
    static GLWidget *getInstance() { return sInstance; }

    PCBoard *getPCB() const { return mPCB.get(); }

    void setPCB(const std::shared_ptr<PCBoard>&);
    void setBottomView(bool b);
    void setLayerAlpha(int z, float a);

    void scheduleSnapshot(const std::string &savePath) { mSnapshotSavePath = savePath; }

    bool areFootprintsShown() const { return mShowFootprints; }
    bool areGridLinesShown() const { return mShowGridLines; }
    bool areGridPointsShown() const { return mShowGridPoints; }
    bool isRatsNestShown() const { return mShowRatsNest; }
    bool isTriangulationShown() const { return mShowTriangulation; }
    bool isRatsNestLimited() const { return mLimitRatsNest; }

    void onKeyRelease(QKeyEvent *);
public slots:
    void animate();
    void resetZoom();
    void focusOn(const Object&);
    void showFootprints(bool b) { mShowFootprints = b; }
    void showGridLines(bool b) { mShowGridLines = b; }
    void showGridPoints(bool b);
    void showRatsNest(bool b) { mShowRatsNest = b; }
    void showTriangulation(bool b) { mShowTriangulation = b; }
    void showGridAStar(bool b);
    void setLimitRatsNest(bool b);

protected:
    void initializeGL() override;

    void paintEvent(QPaintEvent *) override;
    void paintGL() override;
    void paintQt(QPainter *, QPaintEvent *);

    void resizeEvent(QResizeEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;

private:
    float m_dt;
    float m_FPS{0};
    QElapsedTimer mFrameTimer;

    Camera mCamera;

    uint32_t mDirty{0};
    uint32_t ackDirtyMask() { auto m = mDirty; mDirty = 0; return m; }

    std::shared_ptr<PCBoard> mPCB;
    Shaders mShaderCache;
    std::unique_ptr<NavMesh> mNavMesh;
    std::unique_ptr<ui::NavGrid> mNavGrid;
    std::unique_ptr<PCBoardMesh> mBoardMain[2];
    std::unique_ptr<PCBoardMesh> mBoardPins[2];
    std::unique_ptr<RatsNest> mRatsNest;
    std::unique_ptr<RoutePainter> mRoutePainter;
    std::unique_ptr<RoutePainter> mRoutePainterPreview;
    std::unique_ptr<RoutePainterLines> mRoutePainterLines;
    std::unique_ptr<TriList> mTriList;
    std::unique_ptr<ViaPainter> mViaPainter;
    std::unique_ptr<ViaPainter> mViaPainterPreview;
    void resetSceneNodes();

    uint mViewLayer{0};
    bool mShowGridPoints{true};
    bool mShowGridAStar{false};
    bool mShowGridLines{true};
    bool mShowFootprints{true};
    bool mShowRatsNest{true};
    bool mShowTriangulation{true};
    bool mBottomView{false};
    bool mLimitRatsNest{false};

    float mFootprintFillAlphaBase{0.2f};
    float mFootprintLineAlphaBase{1.0f};
    float mPinFillAlphaBase{0.7f};
    float mPinLineAlphaBase{0.0f};

    struct {
        QPoint pos;
        QPoint posDown;
    } mMouse;
    QPointF getNormalizedCursorPos() const;
    QPointF normalizedPos(const QPoint&) const;
    QPoint pixelPos(const QPointF&) const;
    QPoint worldPosToPixels(const Point_2&) const;
    Point_2 pixelsToWorldPos(QPoint) const;

    struct Selection {
        void add(Object *);
        void clearObjects();
        void clear();
        bool setHover(Component *, bool select);
        bool setHover(Pin *);
        QRect rect;
        std::set<Object *> objects;
        struct {
            Component *C{0};
            Pin *P{0};
            Net *N{0};
        } hover;
    } mSelection;
    uint getFirstLayerForSelection() const;
    Object *selectAt(const Point_2&) const;
    void updateHoverSelection();
    void updateRectangleSelection(bool append);
    void clearRectangleSelection();

    UIActions *const mActions;

    void drawInfo(QPainter *);
    void drawCosts(QPainter *);
    void highlightSelected(QPainter&);

    void saveSnapshot();
    std::string mSnapshotSavePath;

private:
    static GLWidget *sInstance;
};

#endif // GYM_PCB_UI_GLWIDGET_H
