
#include "Log.hpp"
#include "GLWidget.hpp"
#include "UI/Actions.hpp"
#include "UI/Window.hpp"
#include "UI/Application.hpp"
#include "PCBoard.hpp"
#include "UI/PCBoardMesh.hpp"
#include "NavGrid.hpp"
#include "UI/NavMesh.hpp"
#include "NavTriangulation.hpp"
#include "UI/TriList.hpp"
#include "Component.hpp"
#include "Net.hpp"
#include "UI/RatsNest.hpp"
#include "UI/RoutePainter.hpp"
#include "UI/RoutePainterLines.hpp"
#include "UI/ViaPainter.hpp"
#include <QApplication>
#include <QPainter>
#include <QTimer>
#include <QMouseEvent>
#include <QImageWriter>

GLWidget *GLWidget::sInstance = 0;

GLWidget *GLWidget::createInstance(Window *win, UIActions *A)
{
    if (sInstance)
        throw std::runtime_error("GLWidget instance already exists");
    sInstance = new GLWidget(win, A);
    return sInstance;
}

void GLWidget::deleteInstance()
{
    DEBUG("GLWidget::deleteInstance " << sInstance);
    assert(sInstance);
    delete sInstance;
    sInstance = 0;
    // NOTE: We cannot set sInstance to 0 in the dtor because member dtors need to use the GL context to free resources.
}

GLWidget::~GLWidget()
{
    DEBUG("~GLWidget");
    assert(sInstance == this);
}

GLWidget::GLWidget(Window *parent, UIActions *A) : QOpenGLWidget(parent), mActions(A)
{
    const auto &conf = UserSettings::get().UI;

    QSurfaceFormat fmt;
    fmt.setRedBufferSize(8);
    fmt.setGreenBufferSize(8);
    fmt.setBlueBufferSize(8);
    //fmt.setAlphaBufferSize(8);
    fmt.setSamples(4);
    setFormat(fmt);

    setMinimumSize(conf.WindowSize[0], conf.WindowSize[1]);
    setAutoFillBackground(false);

    mCamera.setCenter(Point_2(5,5));

    mShowGridLines = conf.GridLinesVisible;
    mShowGridPoints = conf.GridPointsVisible;
    mShowFootprints = conf.FootprintsVisible;
    mShowRatsNest = conf.RatsNestVisible;
    mShowTriangulation = conf.TriangulationVisible;

    mFrameTimer.start();
}

void GLWidget::resetSceneNodes()
{
    mNavMesh.reset(0);
    mNavGrid.reset(0);
    mBoardMain[0].reset(0);
    mBoardMain[1].reset(0);
    mBoardPins[0].reset(0);
    mBoardPins[1].reset(0);
    mRatsNest.reset(0);
    mRoutePainter.reset(0);
    mRoutePainterPreview.reset(0);
    mRoutePainterLines.reset(0);
    mTriList.reset(0);
    mViaPainter.reset(0);
    mViaPainterPreview.reset(0);
}

void GLWidget::setPCB(const std::shared_ptr<PCBoard> &pcb)
{
    INFO("GLWidget::setPCB(" << (pcb ? pcb->name() : "N/A") << ')');

    mSelection.clear();
    mPCB = pcb;
    if (!mPCB)
        return;
    setBottomView(mPCB->getNumLayers() >= 1 ? mBottomView : false);
    mDirty |= PCB_CHANGED_NEW_BOARD;

    const auto &area = mPCB->getComponentAreaBbox();
    auto w = area.xmax() - area.xmin();
    auto h = area.ymax() - area.ymin();
    mCamera.setBaseDimensions(width(), height(), mPCB->metersToUnits(Nanometers(1)));
    mCamera.setView(Point_2(area.xmin() - w / 16, area.ymin() - h / 16), Vector_2(w * 1.125, h * 1.125));
}

QPointF GLWidget::normalizedPos(const QPoint &pixel) const
{
    return QPointF(float(pixel.x()) / width(), 1.0f - float(pixel.y()) / height());
}
QPointF GLWidget::getNormalizedCursorPos() const
{
    return normalizedPos(this->mapFromGlobal(QCursor::pos()));
}
Point_2 GLWidget::pixelsToWorldPos(QPoint v) const
{
    return mCamera.normalizedPosToWorldPos(normalizedPos(v));
}
QPoint GLWidget::pixelPos(const QPointF &pos) const
{
    return QPoint(width() * pos.x(), height() * pos.y());
}
QPoint GLWidget::worldPosToPixels(const Point_2 &v) const
{
    return pixelPos(mCamera.worldPosToNormalizedPos(v));
}

void GLWidget::paintQt(QPainter *painter, QPaintEvent *event)
{
    drawInfo(painter);
    //drawCosts(painter);
    highlightSelected(*painter);
    painter->fillRect(mSelection.rect, QBrush(QColor(255, 255, 255, 64)));
}

void GLWidget::animate()
{
    m_dt = int(mFrameTimer.elapsed()) * 0.001f;
    mFrameTimer.restart();
    m_FPS = m_FPS * 0.96875f + (0.03125f / m_dt);
    update();
}

void GLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
}

void GLWidget::resizeEvent(QResizeEvent *resize)
{
    QOpenGLWidget::resizeEvent(resize);
    mCamera.setBaseDimensions(resize->size().width(), resize->size().height(), mPCB ? mPCB->metersToUnits(Nanometers(1)) : 1.0f);
}

void GLWidget::paintEvent(QPaintEvent *event)
{
    if (mActions)
        mActions->updateIndicators();
    QPainter painter;
    painter.begin(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.beginNativePainting();
    paintGL();
    painter.endNativePainting();
    paintQt(&painter, event);
    painter.end();
}

uint GLWidget::getFirstLayerForSelection() const
{
    switch (mActions->getSelectionLayer()) {
    //case 'T':
    //case 'A':
    default:
        break;
    case 'B': return mPCB->getLayerForSide('b');
    case 'V': return mViewLayer;
    case 'W': return mActions->getActiveLayer();
    }
    return mPCB->getLayerForSide('t');
}
Object *GLWidget::selectAt(const Point_2 &pos) const
{
    if (mActions->getSelectionType() == 'P') {
        auto P = mPCB->getPinAt(pos, getFirstLayerForSelection());
        if (!P && mActions->getSelectionLayer() == 'A')
            P = mPCB->getPinAt(pos,  mPCB->getLayerForSide('b'));
        return P;
    } else {
        auto C = mPCB->getComponentAt(pos, getFirstLayerForSelection());
        if (!C && mActions->getSelectionLayer() == 'A')
            C = mPCB->getComponentAt(pos, mPCB->getLayerForSide('b'));
        return C;
    }
}

bool GLWidget::Selection::setHover(Component *C, bool select)
{
    if (hover.C == C)
        return false;
    if (hover.C && !objects.contains(hover.C))
        hover.C->setSelected(false);
    if (C && select)
        C->setSelected(true);
    hover.C = C;
    return true;
}
bool GLWidget::Selection::setHover(Pin *P)
{
    if (hover.P == P)
        return false;
    if (hover.P && !objects.contains(hover.P))
        hover.P->setSelected(false);
    if (P)
        P->setSelected(true);
    hover.P = P;
    hover.N = P ? P->net() : 0;
    return true;
}
void GLWidget::updateHoverSelection()
{
    const auto routerActive = mActions->getActiveConnections();
    if (mRatsNest && routerActive)
        mRatsNest->setHL(routerActive); // TODO: Add dirty flag.

    auto A = selectAt(mCamera.normalizedPosToWorldPos(getNormalizedCursorPos()));
    auto P = A->as<Pin>();
    auto C = P ? P->getParent() : A->as<Component>();

    mSelection.setHover(C, mActions->getSelectionType() == 'C');
    if (!mSelection.setHover(P))
        return;
    if (mSelection.hover.N) {
        if (mRatsNest && !routerActive)
            mRatsNest->setHL(&mSelection.hover.N->connections());
        if (mRoutePainterLines)
            mRoutePainterLines->update(mSelection.hover.N->connections());
    } else {
        if (mRatsNest && !routerActive)
            mRatsNest->setHL(0);
        if (mRoutePainterLines)
            mRoutePainterLines->update(std::vector<Connection *>());
    }
    if (mRatsNest)
        mDirty |= PCB_CHANGED_NET_COLORS;
}
void GLWidget::Selection::add(Object *A)
{
    A->setSelected(true);
    objects.insert(A);
}
void GLWidget::Selection::clear()
{
    hover.P = 0;
    hover.C = 0;
    hover.N = 0;
    clearObjects();
}
void GLWidget::Selection::clearObjects()
{
    for (auto A : objects)
        A->setSelected(false, 1);
    objects.clear();
}
void GLWidget::clearRectangleSelection()
{
    mSelection.clearObjects();
    if (mActions)
        mActions->setSelection(mSelection.objects);
}
void GLWidget::updateRectangleSelection(bool append)
{
    const auto vBL = mCamera.normalizedPosToWorldPos(normalizedPos(mSelection.rect.bottomLeft()));
    const auto vTR = mCamera.normalizedPosToWorldPos(normalizedPos(mSelection.rect.topRight()));
    const auto bbox = Bbox_2(vBL.x(), vBL.y(), vTR.x(), vTR.y());
    if (!append)
        mSelection.clearObjects();
    for (auto C : mPCB->getComponents()) {
        if ((mActions->getSelectionLayer() == 'A') ? !C->bboxTest(bbox) : !C->bboxTest(bbox, getFirstLayerForSelection()))
            continue;
        if (mActions->getSelectionType() == 'C') {
            mSelection.add(C);
        } else if (mActions->getSelectionType() == 'P') {
            for (auto T : C->getChildren())
                if (T->is_a<Pin>() && T->bboxTest(bbox))
                    mSelection.add(T);
        }
    }
    if (mBoardMain[0]) {
        mBoardPins[0]->setExtraSet(&mSelection.objects);
        mBoardPins[1]->setExtraSet(&mSelection.objects);
        mBoardMain[0]->setExtraSet(&mSelection.objects);
        mBoardMain[1]->setExtraSet(&mSelection.objects);
    }
    if (mRatsNest)
        mDirty |= PCB_CHANGED_NET_COLORS;
    if (mActions)
        mActions->setSelection(mSelection.objects);
}

void GLWidget::paintGL()
{
    if (!mPCB)
        return;
    if (mDirty & PCB_CHANGED_NEW_BOARD)
        resetSceneNodes();

    {
    std::lock_guard rlock(mPCB->getLock());

    if (!mSnapshotSavePath.empty())
        saveSnapshot();

    if (!mBoardMain[0]) {
        const int zT = mPCB->getLayerForSide('t');
        const int zB = mPCB->getLayerForSide('b');
        mBoardMain[0] = std::make_unique<PCBoardMesh>(*mPCB, zT, PCBoardMesh::RENDER_MASK_COMPONENTS);
        mBoardMain[1] = std::make_unique<PCBoardMesh>(*mPCB, zB, PCBoardMesh::RENDER_MASK_COMPONENTS);
        if (!mBoardPins[0]) {
            mBoardPins[0] = std::make_unique<PCBoardMesh>(*mPCB, zT, PCBoardMesh::RENDER_MASK_PINS);
            mBoardPins[1] = std::make_unique<PCBoardMesh>(*mPCB, zB, PCBoardMesh::RENDER_MASK_PINS);
        }
        if (!mBoardMain[0] ||
            !mBoardMain[1] ||
            !mBoardPins[0] ||
            !mBoardPins[1])
            throw std::runtime_error("Could not create graphics objects.");
        setLayerAlpha(zT, 1.0f);
        setLayerAlpha(zB, 1.0f);
    }
    if (!mRoutePainter) {
        mRoutePainter = std::make_unique<RoutePainter>();
        mRoutePainterPreview = std::make_unique<RoutePainter>();
        if (mRoutePainter)
            mRoutePainter->update(*mPCB);
        if (!mViaPainter) {
            mViaPainter = std::make_unique<ViaPainter>();
            mViaPainterPreview = std::make_unique<ViaPainter>();
            if (mViaPainter)
                mViaPainter->update(*mPCB);
        }
        if (!mRoutePainterLines)
            mRoutePainterLines = std::make_unique<RoutePainterLines>(*mPCB);
    }
    if (mShowRatsNest && !mRatsNest) {
        mRatsNest = std::make_unique<RatsNest>(*mPCB);
        if (mRatsNest)
            mRatsNest->setAllVisible(!mLimitRatsNest);
    }
    if (mShowGridLines && !mNavGrid)
        mNavGrid = std::make_unique<ui::NavGrid>(*mPCB);
    if (mShowGridPoints && !mNavMesh) {
        mNavMesh = std::make_unique<NavMesh>(*mPCB);
        if (mNavMesh)
            mNavMesh->setVisualizeAStar(mShowGridAStar);
    }
    if (mShowTriangulation && !mTriList) {
        if (!mPCB->getTNG())
            mPCB->rebuildTNG();
        mTriList = std::make_unique<TriList>(*mPCB);
    }

    if (mActions && mActions->isPreviewReady() && mRoutePainterPreview) {
        mRoutePainterPreview->update(mActions->getRoutePreview(), mPCB->getNumLayers() - 1, mActions->isPreviewTrackLegal() ? Color::WHITE : Color::WHITE.withAlpha(0.5f));
        if (mViaPainterPreview)
            mViaPainterPreview->update(mActions->getRoutePreview());
        mActions->discardPreview();
    }

    const uint32_t mask = mPCB->ackChanges() | ackDirtyMask();
    if (mask) {
        if (mask & PCB_CHANGED_OBJECTS) {
            clearRectangleSelection();
            if (mRatsNest)
                mRatsNest->setHL(0);
        }
        if ((mask & PCB_CHANGED_NAV_GRID) && mShowGridPoints) {
            if (mNavMesh)
                mNavMesh->update(mActions->getActiveLayer());
        }
        if (mask & PCB_CHANGED_ROUTES) {
            if (mRoutePainter) {
                mRoutePainter->update(*mPCB);
                if (mViaPainter)
                    mViaPainter->update(*mPCB);
            }
            if (mRoutePainterLines && mSelection.hover.N)
                mRoutePainterLines->update(mSelection.hover.N->connections());
            if (mRatsNest) {
                mRatsNest->update();
                if (mSelection.hover.N)
                    mRatsNest->setHL(&mSelection.hover.N->connections());
            }
        }
        if (mask & PCB_CHANGED_NET_COLORS) {
            if (mRatsNest)
                mRatsNest->update();
        }
        if (mBoardMain[0] && (mask & PCB_CHANGED_COMPONENTS)) {
            mBoardMain[0]->update();
            if (mPCB->getNumLayers() > 1)
                mBoardMain[1]->update();
        }
        if (mBoardPins[0] && (mask & PCB_CHANGED_PINS)) {
            mBoardPins[0]->update();
            if (mPCB->getNumLayers() > 1)
                mBoardPins[1]->update();
        }
        if (mTriList && (mask & PCB_CHANGED_NAV_TRIS))
            mTriList->update(*mPCB);
    }
    } // release shared PCB lock

    mCamera.loadUniforms();

    glClearColor(0.125f, 0.125f, 0.125f, 1.0f);
    glClearStencil(0x0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (mBoardMain[0]) {
        if (mPCB->getNumLayers() > 1) {
            mBoardMain[mBottomView ? 0 : 1]->draw(mCamera);
            mBoardMain[mBottomView ? 0 : 1]->drawExtra();
            mBoardPins[mBottomView ? 0 : 1]->draw(mCamera);
            mBoardPins[mBottomView ? 0 : 1]->drawExtra();
        }
        mBoardMain[mBottomView ? 1 : 0]->draw(mCamera);
        mBoardMain[mBottomView ? 1 : 0]->drawExtra();
    }
    if (mShowGridLines && mNavGrid)
        mNavGrid->draw(mCamera);
    if (mRoutePainter) {
        mRoutePainter->draw(mCamera);
        if (mViaPainter)
            mViaPainter->draw(mCamera);
        if (mActions && !mActions->isPreviewStale()) {
            mRoutePainterPreview->draw(mCamera);
            mViaPainterPreview->draw(mCamera);
        }
    }
    if (mRoutePainterLines)
        mRoutePainterLines->draw(mCamera);
    if (mBoardPins[0]) {
        mBoardPins[mBottomView ? 1 : 0]->draw(mCamera);
        mBoardPins[mBottomView ? 1 : 0]->drawExtra();
    }
    if (mShowRatsNest && mRatsNest)
        mRatsNest->draw(mCamera);
    if (mShowTriangulation && mTriList)
        mTriList->draw(mCamera);
    if (mShowGridPoints && mNavMesh)
        mNavMesh->draw(mCamera);
}


void GLWidget::drawCosts(QPainter *painter)
{
    if (!mPCB)
        return;
    QFont font;
    font.setPixelSize(24);
    QBrush brush(QColor(0x20, 0x20, 0x20, 0xd0));
    QPen textPen(Qt::white);
    painter->setBrush(brush);
    painter->setPen(textPen);
    painter->setFont(font);
    const auto &nav = mPCB->getNavGrid();
    for (auto &P : nav.getPoints()) {
        auto pos = worldPosToPixels(P.getRefPoint(&nav));
        pos.setX(pos.x() - 20);
        pos.setY(pos.y() + 12);
        const auto v = P.getCost();
        painter->drawText(pos, QString(std::to_string(v).substr(0,3).c_str()));
    }
}

void GLWidget::drawInfo(QPainter *painter)
{
    if (!mPCB)
        return;
    const Point_2 boardPos = mCamera.normalizedPosToWorldPos(getNormalizedCursorPos());

    std::string info[4];
    {
    std::lock_guard rlock(mPCB->getLock());
    updateHoverSelection();
    auto C = mSelection.hover.C;
    auto P = mSelection.hover.P;
    if (C) {
        info[0] = fmt::format("Component: {} [{}] z={}", C->name(), geo::to_string(C->getBbox()), C->getSingleLayer());
        if (P) {
            info[1] = fmt::format("Pin: {} [{}] z=[{},{}]", P->name(), geo::to_string(P->getBbox()), P->minLayer(), P->maxLayer());
            if (P->net())
                info[2] = fmt::format("\nNet: {}", P->net()->name());
        }
    } else {
        info[0] = "Empty";
    }
    const IVector_2 gridPos = mPCB->getNavGrid().HIndices(boardPos);
    info[3] = fmt::format("Pos: ({:.4}, {:.4}) Grid: ({}, {}) / FPS: {:.1f}", boardPos.x(), boardPos.y(), gridPos.x, gridPos.y, m_FPS);
    } // LOCK GUARD

    if (mShowTriangulation)
        info[3] += std::string(" / Tri ") + std::to_string(mPCB->getTNG()->getNavIdx(boardPos, 0));

    QFont font;
    font.setPixelSize(18);
    QBrush brush(QColor(0x20, 0x20, 0x20, 0xd0));
    QPen textPen(Qt::white);
    QPen rectPen(Qt::white);
    painter->setPen(rectPen);
    painter->setBrush(brush);
    painter->drawRect(0, height() - 128, 512, 128);
    painter->setPen(textPen);
    painter->setFont(font);
    const int y0 = height() - 128 - 8;
    painter->drawText(QPoint(16, y0 +  32), QString(info[0].c_str()));
    painter->drawText(QPoint(16, y0 +  64), QString(info[1].c_str()));
    painter->drawText(QPoint(16, y0 +  96), QString(info[2].c_str()));
    painter->drawText(QPoint(16, y0 + 128), QString(info[3].c_str()));
}

void GLWidget::onKeyRelease(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Shift) {
        event->accept();
        if (mActions)
            mActions->setRouteMode(mActions->getRouteMode() ? 0 : mActions->getLastRouteMode());
        setMouseTracking(mActions->getRouteMode() != 0);
    } else {
        event->ignore();
    }
}

void GLWidget::mouseMoveEvent(QMouseEvent *event)
{
    const auto move = event->pos() - mMouse.pos;
    mMouse.pos = event->pos();

    const float s = mCamera.getDims().x() / float(this->width());

    if (event->buttons() == Qt::RightButton) {
        // Scroll
        mCamera.moveBy(Vector_2(-move.x() * s, move.y() * s));
    } else if (mActions->getRouteMode() != 0) {
        if (mActions && mActions->isPreviewing())
            mActions->routePreview(pixelsToWorldPos(mMouse.pos));
    } else if (event->buttons() == Qt::LeftButton) {
        // Select
        const auto dims = mMouse.pos - mMouse.posDown;
        auto x = std::min(mMouse.posDown.x(), mMouse.pos.x());
        auto y = std::min(mMouse.posDown.y(), mMouse.pos.y());
        auto w = std::abs(dims.x());
        auto h = std::abs(dims.y());
        if (w < 4 || h < 4)
            w = h = 0;
        mSelection.rect = QRect(x, y, w, h);
    } else {
        QWidget::mouseMoveEvent(event);
    }
}
void GLWidget::mousePressEvent(QMouseEvent *event)
{
    mMouse.posDown = event->pos();
    mMouse.pos = event->pos();
}
void GLWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (mActions->getRouteMode() != 0 && event->button() == Qt::LeftButton) {
        if (mActions)
            mActions->routeTo(pixelsToWorldPos(mMouse.posDown));
    } else if (!mSelection.rect.isEmpty()) {
        if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
            updateRectangleSelection(QApplication::keyboardModifiers() & Qt::AltModifier);
        } else {
            float zoom = float(mSelection.rect.width()) / this->width();
            float x = mSelection.rect.x(), w = width();
            float y = mSelection.rect.y(), h = height();
            y += mSelection.rect.height();
            //Point_2 O = mCamera.getOrigin();
            Vector_2 Or = Vector_2(x / w, 1.0f - y / h);
            mCamera.setOriginRelative(Or);
            mCamera.setZoom(zoom * mCamera.getZoom(), QPointF(0,0));
        }
        mSelection.rect = QRect(0,0,0,0);
    } else if (mActions && event->button() == Qt::LeftButton) {
        std::lock_guard rlock(mPCB->getLock());
        auto target = selectAt(pixelsToWorldPos(mMouse.posDown));
        mActions->setSelection(target);
    } else if (event->button() == Qt::MiddleButton) {
        if (mActions)
            mActions->routeTo(pixelsToWorldPos(mMouse.posDown));
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}
void GLWidget::wheelEvent(QWheelEvent *event)
{
    auto dAy = event->angleDelta().y();
    if (dAy > 0)
        mCamera.setZoom(mCamera.getZoom() * 1.1f);
    else if (dAy < 0)
        mCamera.setZoom(mCamera.getZoom() * 0.9f);
}

void GLWidget::resetZoom()
{
    mCamera.setView(mPCB->getLayoutArea().origin(), mPCB->getLayoutArea().size());
}

void GLWidget::focusOn(const Object &A)
{
    const auto box = A.getBbox();
    const auto win =
        Vector_2(std::min(mPCB->getLayoutArea().size().x(), 8 * (box.xmax() - box.xmin())),
                 std::min(mPCB->getLayoutArea().size().y(), 8 * (box.ymax() - box.ymin())));
    mCamera.setView(A.getCenter() - win * 0.5, win);
}

void GLWidget::highlightSelected(QPainter &painter)
{
    Object *A = 0;
    if (mActions->getSelectionType() == 'P')
        A = mSelection.hover.P;
    else if (mActions->getSelectionType() == 'C')
        A = mSelection.hover.C;
    if (!A)
        return;
    const auto bbox = A->getBbox();
    auto vLL = worldPosToPixels(Point_2(bbox.xmin(), bbox.ymin()));
    auto vTR = worldPosToPixels(Point_2(bbox.xmax(), bbox.ymax()));
    QBrush brush(QColor(0xff, 0xff, 0xff, 0x40));
    QPen pen(QColor(0xff, 0xff, 0xff, 0xc0));
    painter.setPen(pen);
    painter.setBrush(brush);
    painter.drawRect(vLL.x(), vLL.y(), vTR.x() - vLL.x(), vTR.y() - vLL.y());
}

void GLWidget::setLayerAlpha(int z, float a)
{
    if (z == 0) {
        mBoardMain[0]->setFillAlpha(a * mFootprintFillAlphaBase);
        mBoardMain[0]->setLineAlpha(a * mFootprintLineAlphaBase);
        mBoardPins[0]->setFillAlpha(a * mPinFillAlphaBase);
        mBoardPins[0]->setLineAlpha(a * mPinLineAlphaBase);
        if (mRatsNest)
            mRatsNest->setLayerAlpha(z, a);
    } else if (z == int(mPCB->getLayerForSide('b'))) {
        mBoardMain[1]->setFillAlpha(a * mFootprintFillAlphaBase);
        mBoardMain[1]->setLineAlpha(a * mFootprintLineAlphaBase);
        mBoardPins[1]->setFillAlpha(a * mPinFillAlphaBase);
        mBoardPins[1]->setLineAlpha(a * mPinLineAlphaBase);
        if (mRatsNest)
            mRatsNest->setLayerAlpha(z, a);
    }
    if (mRoutePainter)
        mRoutePainter->setLayerAlpha(z, a);
    if (mRatsNest)
        mDirty |= PCB_CHANGED_NET_COLORS;
}

void GLWidget::setBottomView(bool b)
{
    if (b && mPCB && mPCB->getNumLayers() <= 1)
        throw std::runtime_error("Cannot set bottom view on single-layer board");
    mBottomView = b;
    if (mPCB)
        mViewLayer = mPCB->getLayerForSide(b ? 'b' : 't');
}

void GLWidget::showGridPoints(bool b)
{
    mShowGridPoints = b;
    if (mShowGridPoints && mNavMesh)
        mDirty |= PCB_CHANGED_NAV_GRID;
}
void GLWidget::showGridAStar(bool b)
{
    mShowGridAStar = b;
    if (mNavMesh) {
        mNavMesh->setVisualizeAStar(b);
        if (mShowGridPoints)
            mDirty |= PCB_CHANGED_NAV_GRID;
    }
}

void GLWidget::setLimitRatsNest(bool b)
{
    mLimitRatsNest = b;
    if (mRatsNest) {
        mRatsNest->setAllVisible(!b);
        mDirty |= PCB_CHANGED_NET_COLORS;
    }
}

void GLWidget::saveSnapshot()
{
    assert(!mSnapshotSavePath.empty());
    const char *savePath = mSnapshotSavePath.c_str();
    WARN("Saving snapshot to: " << savePath);
    struct {
        GLint fb;
        GLint vp[4];
    } save;
    glGetIntegerv(GL_VIEWPORT, save.vp);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &save.fb);
    const uint NL = mPCB->getNumLayers();
    const float vAr = float(save.vp[2]) / save.vp[3];
    const float ar = mPCB->getLayoutArea().aspectRatio();
    GLint w, h, vw, vh;
    if (ar < vAr) {
        h = vh = 1080;
        w = int(h * ar);
        vw = int(h * vAr);
    } else {
        w = vw = 1920;
        h = int(w / ar);
        vh = int(w / vAr);
    }
    const uint stride = w * h * 3;
    const uint size = stride * NL;
    WARN("Image dimensions: " << w << 'x' << h << 'x' << NL << " = " << size << " bytes");
    uchar *data = new uchar[size];
    if (!data)
        return;

    GLuint fbo, cbo, zbo;
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &cbo);
    glGenRenderbuffers(1, &zbo);
    glBindRenderbuffer(GL_RENDERBUFFER, cbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, vw, vh);
    glBindRenderbuffer(GL_RENDERBUFFER, zbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, vw, vh);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, cbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, zbo);
    glViewport(0, 0, vw, vh);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearStencil(0x0);
    glClearDepth(0.0f);

    for (uint i = 0; i < NL; ++i) {
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (mRoutePainter)
            mRoutePainter->drawLayer(mCamera, NL - i - 1);
        glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, &data[i * stride]);
    }
    QImage image(data, w, h * NL, QImage::Format_RGB888);
    image = image.flipped(Qt::Vertical);
    QImageWriter writer(savePath, "png");
    writer.write(image);
    delete[] data;
    mSnapshotSavePath.clear();

    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &cbo);
    glDeleteRenderbuffers(1, &zbo);
    glBindFramebuffer(GL_FRAMEBUFFER, save.fb);
    glViewport(save.vp[0], save.vp[1], save.vp[2], save.vp[3]);
}
