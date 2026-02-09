
#include "Log.hpp"
#include "UserSettings.hpp"
#include "PCBoard.hpp"
#include "UI/ViewTab.hpp"
#include "UI/GLWidget.hpp"
#include <QCheckBox>
#include <QFileDialog>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include "Qt/Loader.hpp"

LayerAlphaSlider::LayerAlphaSlider(ViewTab *V, int z) : mZ(z)
{
    auto L = std::make_unique<QLabel>((z >= 0) ? QString::number(z) : QString("*"));
    auto S = std::make_unique<QSlider>(Qt::Horizontal);
    if (!L || !S)
        return;
    L->setMinimumWidth(12);
    S->setMinimum(0);
    S->setMaximum(100);
    S->setValue(100);
    mSlider = S.get();
    QObject::connect(mSlider, &QSlider::valueChanged, [V, z](int v) { V->onLayerAlphaSlider(z, v); });
    addWidget(L.release());
    addWidget(S.release());
}
LayerAlphaSlider::~LayerAlphaSlider()
{
    for (auto I = takeAt(0); I; I = takeAt(0)) {
        delete I->widget();
        delete I;
    }
}

bool ViewTab::init(GLWidget *_WGL)
{
    WGL = _WGL;
    auto W = LoadUIFile(this, "view_tab.ui");
    if (!W)
        return false;
    mResetZoom = W->findChild<QPushButton *>("PResetZoom");
    mShowGridLines = W->findChild<QCheckBox *>("CShowLines");
    mShowGridPoints = W->findChild<QCheckBox *>("CShowPoints");
    mShowGridPoints = W->findChild<QCheckBox *>("CShowPoints");
    mShowAStar = W->findChild<QCheckBox *>("CAStar");
    mShowRatsNest = W->findChild<QCheckBox *>("CShowRats");
    mLimitRatsNest = W->findChild<QCheckBox *>("CLimitRats");
    mShowTriangles = W->findChild<QCheckBox *>("CShowTris");
    mSelectionLayers = W->findChild<QCheckBox *>("CSelectLayers");
    mSelectionType = W->findChild<QCheckBox *>("CSelectParts");
    mScreenshot = W->findChild<QPushButton *>("PScreenshot");
    mZDirection = W->findChild<QPushButton *>("PFlipZ");
    mAlphaSliders = W->findChild<QVBoxLayout *>("VLayerAlpha");
    mSlowdown = W->findChild<QSlider *>("ZSlowdown");
    mSlowdownLabel = W->findChild<QLabel *>("LSlowdown");
    auto SaveButton = W->findChild<QPushButton *>("PSave");
    assert(mResetZoom);
    assert(mShowGridLines);
    assert(mShowGridPoints && mShowAStar);
    assert(mShowRatsNest);
    assert(mLimitRatsNest);
    assert(mShowTriangles);
    assert(mZDirection);
    assert(mAlphaSliders);
    assert(mSlowdown && mSlowdownLabel);
    assert(SaveButton);
    mShowGridLines->setChecked(WGL->areGridLinesShown());
    mShowGridPoints->setChecked(WGL->areGridPointsShown());
    mShowRatsNest->setChecked(WGL->isRatsNestShown());
    mLimitRatsNest->setChecked(WGL->isRatsNestLimited());
    mShowTriangles->setChecked(WGL->isTriangulationShown());
    mSlowdown->setSliderPosition(slowdownToSliderPosition(UserSettings::get().UI.ActionDelayUSecs));
    onSlowdownSlider(mSlowdown->sliderPosition());
    QObject::connect(mResetZoom, &QPushButton::clicked, WGL, &GLWidget::resetZoom);
    QObject::connect(mShowGridLines, &QCheckBox::clicked, WGL, &GLWidget::showGridLines);
    QObject::connect(mShowGridPoints, &QCheckBox::clicked, WGL, &GLWidget::showGridPoints);
    QObject::connect(mShowRatsNest, &QCheckBox::clicked, WGL, &GLWidget::showRatsNest);
    QObject::connect(mLimitRatsNest, &QCheckBox::clicked, WGL, &GLWidget::setLimitRatsNest);
    QObject::connect(mShowTriangles, &QCheckBox::clicked, WGL, &GLWidget::showTriangulation);
    QObject::connect(mShowAStar, &QCheckBox::clicked, WGL, &GLWidget::showGridAStar);
    QObject::connect(mSlowdown, &QSlider::valueChanged, this, &ViewTab::onSlowdownSlider);
    QObject::connect(mZDirection, &QPushButton::clicked, [this](){ WGL->setBottomView(changeDirection()); });
    QObject::connect(SaveButton, &QPushButton::clicked, this, &ViewTab::onSave);
    return true;
}

bool ViewTab::changeDirection()
{
    auto b = mZDirection->text()[0] == "B";
    mZDirection->setText(b ? "Top View" : "Bottom View");
    return !b;
}

void ViewTab::onSlowdownSlider(int v)
{
    assert(v >= 0);
    const uint us = v * v * v * 5;
    mSlowdownLabel->setText("Operation Slowdown: " + QString::fromStdString(Logger::formatDurationUS(us)));
    UserSettings::edit().UI.ActionDelayUSecs = us;
}
uint ViewTab::slowdownToSliderPosition(uint us) const
{
    return int(std::cbrt(us / 5));
}

void ViewTab::setNumLayers(GLWidget *WGL, uint NL)
{
    if (!mAlphaSliders)
        return;
    for (auto I = mAlphaSliders->takeAt(0); I; I = mAlphaSliders->takeAt(0)) {
        delete I->widget();
        delete I;
    }
    for (uint z = 0; z <= NL; ++z) {
        auto A = new LayerAlphaSlider(this, (z < NL) ? z : -1);
        if (A)
            mAlphaSliders->addLayout(A);
    }
    if (NL) {
        mAlphaSliders->addStretch();
        mZDirection->setEnabled(NL > 1);
        if (NL == 1)
            mZDirection->setText("Top View");
    }
}

void ViewTab::onLayerAlphaSlider(int z, int v)
{
    if (z >= 0) {
        WGL->setLayerAlpha(z, v * (1.0f / 100.0f));
    } else {
        for (z = 0; z < mAlphaSliders->count(); ++z) {
            auto I = mAlphaSliders->itemAt(z);
            if (!I)
                break;
            auto L = dynamic_cast<LayerAlphaSlider *>(I->layout());
            if (L && L->getLayer() >= 0)
                L->getSlider()->setValue(v);
        }
    }
}

void ViewTab::onSave()
{
    auto filePath = QFileDialog::getSaveFileName(this, "Select destination").toStdString();
    if (filePath.empty())
        return;

    py::GIL_guard GIL;
    py::ObjectRef M(PyImport_Import(**py::ObjectRef(py::String("json"))));
    if (!*M)
        return;
    py::ObjectRef f(PyObject_GetAttrString(**M, "dumps"));
    if (!*f)
        return;
    py::ObjectRef none(py::ObjectRef(PyTuple_New(0)));
    py::ObjectRef args(PyDict_New());
    args->setItem("obj", WGL->getPCB()->getPy(0, 4, false));
    args->setItem("indent", PyLong_FromLong(2));
    py::ObjectRef json(PyObject_Call(**f, **none, **args));

    std::ofstream of(filePath);
    of << json->asString();
    of.close();
    NOTICE("Saved JSON board to " << filePath);
}
