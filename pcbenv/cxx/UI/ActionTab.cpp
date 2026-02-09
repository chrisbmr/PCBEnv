
#include "Log.hpp"
#include "UI/ActionTab.hpp"
#include "UI/Actions.hpp"
#include "UI/Qt/Loader.hpp"
#include "UI/Window.hpp"
#include "PCBoard.hpp"
#include "Pin.hpp"
#include "UserSettings.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QSpinBox>
#include <QToolButton>

bool ActionTab::init()
{
    auto W = LoadUIFile(this, "action_tab.ui");
    if (!W)
        return false;
    mPreview = W->findChild<QCheckBox *>("CPreview");
    mStepLock = W->findChild<QCheckBox *>("CLock");
    mLayerChoice = W->findChild<QComboBox *>("DLayer");
    mTimeoutUnit = W->findChild<QComboBox *>("DTimeout");
    mSelectType = W->findChild<QComboBox *>("DSelectType");
    mSelectZ = W->findChild<QComboBox *>("DSelectLayer");
    mSelection = W->findChild<QListWidget *>("LSelection");
    mTimeoutSpin = W->findChild<QSpinBox *>("STimeout");
    mAutoroute = W->findChild<QPushButton *>("PAutoroute");
    mProgress = W->findChild<QProgressBar *>("RProgress");
    mConnect = W->findChild<QPushButton *>("PConnect");
    mInterrupt = W->findChild<QPushButton *>("PInterrupt");
    mUnrouteS = W->findChild<QPushButton *>("PUnrouteS");
    mUnrouteX = W->findChild<QPushButton *>("PUnrouteX");
    mViaCost = W->findChild<QDoubleSpinBox *>("SViaCost");
    mExit = W->findChild<QPushButton *>("PExit");
    mTools[0] = W->findChild<QToolButton *>("TRoute");
    mTools[1] = W->findChild<QToolButton *>("TSegment");
    mUndoTool = W->findChild<QToolButton *>("TUndo");
    mNextStep = W->findChild<QToolButton *>("TStep");
    assert(mPreview);
    assert(mLayerChoice);
    assert(mTimeoutSpin && mTimeoutUnit);
    assert(mSelectType);
    assert(mSelectZ);
    assert(mSelection);
    assert(mAutoroute);
    assert(mProgress);
    assert(mConnect);
    assert(mInterrupt);
    assert(mUnrouteS && mUnrouteX);
    assert(mViaCost);
    assert(mExit);
    assert(mTools[0] && mTools[1]);
    assert(mUndoTool);
    assert(mStepLock && mNextStep);
    return true;
}

void ActionTab::connect(Window &W, UIActions &A)
{
    mActions = &A;

    mPreview->setChecked(A.isPreviewing());

    QObject::connect(mPreview, &QCheckBox::clicked, [this](bool b){ mActions->setPreview(b); });
    QObject::connect(mAutoroute, &QPushButton::clicked, [this]() { mActions->autoroute(getTimeoutMS()); });
    QObject::connect(mConnect, &QPushButton::clicked, [this](){ mActions->connect(); });
    QObject::connect(mInterrupt, &QPushButton::clicked, [this](){ mActions->interrupt(); });
    QObject::connect(mUnrouteS, &QPushButton::clicked, [this](){ mActions->unrouteSelection(); });
    QObject::connect(mUnrouteX, &QPushButton::clicked, [this](){ mActions->unrouteConnection(); });
    QObject::connect(mUndoTool, &QToolButton::clicked, [this](){ mActions->unrouteSegment(); });
    QObject::connect(mViaCost, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double v){ mActions->setViaCost(v); });
    QObject::connect(mTools[0], &QToolButton::clicked, [this, &W](){ onClickTool('*', W); });
    QObject::connect(mTools[1], &QToolButton::clicked, [this, &W](){ onClickTool('s', W); });
    QObject::connect(mSelectType, &QComboBox::currentTextChanged, [this](const QString &s){ mActions->setSelectionType(s[0].toLatin1()); });
    QObject::connect(mSelectZ, &QComboBox::currentTextChanged, [this](const QString &s){ mActions->setSelectionLayer(s[0].toLatin1()); });
    QObject::connect(mLayerChoice, &QComboBox::currentTextChanged, [this](){ mActions->setActiveLayer(getLayer()); });
    QObject::connect(mExit, &QPushButton::clicked, QApplication::instance(), &QApplication::quit);
    QObject::connect(mStepLock, &QCheckBox::clicked, [this](bool b){ mActions->setLockStepping(b ? 1 : 0); });
    QObject::connect(mNextStep, &QCheckBox::clicked, [this](){ mActions->nextStep(); });

    mViaCost->setValue(UserSettings::get().AStarViaCostFactor);
}

uint64_t ActionTab::getTimeoutMS() const
{
    const auto v = mTimeoutSpin->value();
    const auto u = mTimeoutUnit->currentText();
    if (u == "h") return v * (3600 * 1000);
    if (u == "m") return v * (  60 * 1000);
    if (u == "s") return v * (   1 * 1000);
    return v;
}

void ActionTab::setNumLayers(uint NL)
{
    while (uint(mLayerChoice->count()) > NL)
        mLayerChoice->removeItem(mLayerChoice->count() - 1);
    while (uint(mLayerChoice->count()) < NL)
        mLayerChoice->addItem(QString().setNum(mLayerChoice->count()));
}

uint ActionTab::getLayer() const
{
    return mLayerChoice->currentText().toUInt();
}

void ActionTab::setPins(const Pin *P0, const Pin *P1)
{
    std::string s = "Connect ";
    s += P0 ? P0->getFullName() : "?";
    s += " to ";
    s += P1 ? P1->getFullName() : "?";
    mConnect->setText(s.c_str());
}

void ActionTab::setActiveTool(char t)
{
    const uint n = (t == '*') ? 0 : ((t == 's') ? 1 : 2);
    for (uint i = 0; i < 2; ++i)
        mTools[i]->setDown(i == n);
}
void ActionTab::onClickTool(char c, Window &W)
{
    mActions->setRouteMode(mActions->getRouteMode() == c ? 0 : c);
    if (W.getGLWidget())
        W.getGLWidget()->setMouseTracking(!!mActions->getRouteMode());
}

void ActionTab::newSelections()
{
    while (auto I = mSelection->takeItem(0))
        delete I;
    for (const auto *C : mActions->getSelectionSetC())
        mSelection->addItem(QString::fromStdString(C->name()));
    for (const auto *P : mActions->getSelectionSetP())
        mSelection->addItem(QString::fromStdString(P->getFullName()));
}

void ActionTab::setBoard(const PCBoard *PCB)
{
}

void ActionTab::setProgress(uint steps)
{
    mProgress->setValue(std::min(steps, 100u));
}
