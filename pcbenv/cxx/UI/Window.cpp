
#include "Log.hpp"
#include "UI/Window.hpp"
#include "UI/Application.hpp"
#include "UI/Actions.hpp"
#include "UI/GLWidget.hpp"
#include "UI/ActionTab.hpp"
#include "UI/BrowserTab.hpp"
#include "UI/ParameterTab.hpp"
#include "UI/ViewTab.hpp"
#include "RL/Agent.hpp"
#include "PCBoard.hpp"
#include "UserSettings.hpp"
#include <QApplication>
#include <QStyle>
#include <QTimer>
#include <QLabel>
#include <QGridLayout>
#include <QKeyEvent>
#include <QTabWidget>

Window::Window(UIApplication &UIA) : mUIA(UIA)
{
    setWindowTitle(tr("PCB Gym"));

    // FIXME: Lifecycle of UIActions and GLWidget and ActionTab?
    mActions = std::make_unique<UIActions>(mUIA);
    GLWidget::createInstance(this, mActions.get());

    mViewTab = new ViewTab();
    mViewTab->init(getGLWidget());

    mActionTab = new ActionTab();
    mActionTab->init();
    mActionTab->connect(*this, *mActions);
    mActions->setUIElement(mActionTab);

    mBrowserTab = new BrowserTab();
    mBrowserTab->init();
    mBrowserTab->connect(*getGLWidget());

    mParamTab = new ParameterTab();
    mParamTab->init();

    mTabContainer = std::make_unique<QTabWidget>(this);
    mTabContainer->addTab(mActionTab, "Actions");
    mTabContainer->addTab(mViewTab, "View");
    mTabContainer->addTab(mBrowserTab, "Browser");
    mTabContainer->addTab(mParamTab, "⚙️");

    auto layout = new QGridLayout();
    layout->setColumnStretch(0, 1);
    layout->setColumnMinimumWidth(1, UserSettings::get().UI.SidePaneWidth);
    layout->addWidget(getGLWidget(), 0, 0);
    layout->addWidget(mTabContainer.get(), 0, 1);
    setLayout(layout);

    mTimer = std::make_unique<QTimer>(this);
    connect(mTimer.get(), &QTimer::timeout, getGLWidget(), &GLWidget::animate);
    mTimer->start(1000 / UserSettings::get().UI.FPS);
}

Window::~Window()
{
    DEBUG("~Window");
    // FIXME: mTabContainer gets deleted twice even if we do "while(layout()->takeAt(0))".
    mTabContainer.release();
    GLWidget::deleteInstance();
}

void Window::setPCB(const std::shared_ptr<PCBoard> &pcb)
{
    const uint NL = pcb ? pcb->getNumLayers() : 0;
    getGLWidget()->setPCB(pcb);
    mActions->reset();
    mActionTab->setNumLayers(NL);
    mBrowserTab->setBoard(pcb.get());
    mViewTab->setNumLayers(getGLWidget(), NL);
}

void Window::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    default:
        QWidget::keyPressEvent(event);
        break;
    case Qt::Key_S:
        mActions->nextStep();
        break;
    }
}
void Window::keyReleaseEvent(QKeyEvent *event)
{
    getGLWidget()->onKeyRelease(event);
    if (!event->isAccepted())
        QWidget::keyReleaseEvent(event);
}
