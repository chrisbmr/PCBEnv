
#include "Log.hpp"
#include "UserSettings.hpp"
#include "UI/Window.hpp"
#include "UI/LockStep.hpp"
#include "UI/ParameterTab.hpp"
#include "PCBoard.hpp"
#include "Net.hpp"
#include "Component.hpp"
#include "RL/Agent.hpp"
#include <QApplication>
#include <QSurfaceFormat>
#include "QApp.hpp"

using Completion = StepLock;

IUIApplication *IUIApplication::create()
{
    return new QPCBApplication();
}

QPCBApplication::QPCBApplication()
{
    argv[0] = new char[8];
    if (argv[0])
        strcpy(argv[0], "PCB Gym");

    mUserAgent.reset(Agent::create(""));
    if (mUserAgent)
        mUserAgent->getStepLock().setGranularity(1); // for render("wait")
}
QPCBApplication::~QPCBApplication()
{
    QApplication::quit();
    if (mThread.joinable())
        mThread.join();
    delete[] argv[0];
}

class SynchronizedQEvent : public QEvent
{
public:
    SynchronizedQEvent(Completion *s = 0) : signal(s), QEvent(QEvent::User) { }
    ~SynchronizedQEvent() override { if (signal) signal->signal(); }
private:
    Completion *signal;
};

class SetPCBEvent : public QEvent
{
public:
    SetPCBEvent(const std::shared_ptr<PCBoard> &_PCB, bool task) : PCB(_PCB), TaskActive(task), QEvent(QEvent::User) { }
    std::shared_ptr<PCBoard> PCB;
    const bool TaskActive;
};
void QPCBApplication::setPCB(const std::shared_ptr<PCBoard> &pcb, bool wait)
{
    if (wait)
        startExclusiveTask("Waiting to change UI's PCB");
    QApplication::postEvent(mEventReceiver.get(), new SetPCBEvent(pcb, wait));
}

class TerminateEvent : public QEvent
{
public:
    TerminateEvent(int code) : exitCode(code), QEvent(QEvent::User) { }
    int exitCode;
};
void QPCBApplication::quit(int code)
{
    // We use an event as this must happen on the UI thread.
    QApplication::postEvent(mEventReceiver.get(), new TerminateEvent(code));
    if (mThread.joinable())
        mThread.join();
}

class SetAutoAgentEvent : public SynchronizedQEvent
{
public:
    SetAutoAgentEvent(const std::shared_ptr<Agent> &A, Completion *s = 0) : agent(A), SynchronizedQEvent(s) { }
    std::shared_ptr<Agent> agent;
};
void QPCBApplication::setAutonomousAgent(const std::shared_ptr<Agent> &A,
                                         const std::shared_ptr<PCBoard> &PCB)
{
    startExclusiveTask("Waiting to change UI agent while UI agent is busy");
    A->setPCB(PCB);
    endExclusiveTask();
    Completion sync(1);
    QApplication::postEvent(mEventReceiver.get(), new SetAutoAgentEvent(A, &sync));
    Py_BEGIN_ALLOW_THREADS
    sync.wait();
    Py_END_ALLOW_THREADS
}

void QPCBApplication::proc()
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QSurfaceFormat fmt;
    fmt.setStencilBufferSize(8);
    fmt.setSamples(4);
    if (!UserSettings::get().UI.VSync)
        fmt.setSwapInterval(0);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    // std::stod depends on the locale and if that's German it suddenly expects ',' for the decimal point.
    QLocale::setDefault(QLocale::c());

    mEventReceiver = std::make_unique<QPCBEventReceiver>(*this);
    if (!mEventReceiver)
        return;
    mWindow = std::make_unique<Window>(*this);
    mPromise->set_value(!!mWindow);
    delete mPromise;
    if (!mWindow)
        return;
    mWindow->show();

    app.setQuitOnLastWindowClosed(false);
    INFO("Starting QApplication.");
    app.exec();
    INFO("QApplication exited.");

    mWindow.reset();
}

std::future<bool> QPCBApplication::runAsync()
{
    INFO("Launching UI thread.");
    mPromise = new std::promise<bool>();
    new(&mThread) std::thread(std::bind(&QPCBApplication::proc, this));
    return mPromise->get_future();
}

// We cannot quit QApplication and restart it on a different thread,
// so we need to keep it running and only close the window.
void QPCBApplication::hide()
{
    QCoreApplication::postEvent(mEventReceiver.get(), new QEvent(QEvent::Hide));
}
void QPCBApplication::show()
{
    QCoreApplication::postEvent(mEventReceiver.get(), new QEvent(QEvent::Show));
}

void QPCBApplication::_setPCB(const std::shared_ptr<PCBoard> &PCB, bool lockedAlready)
{
    INFO("QPCBApplication setPCB = " << PCB.get());

    if (!lockedAlready)
        startExclusiveTask("Changing PCB while a task/agent is running will block the UI");
    mPCB = PCB;
    mUserAgent->setPCB(PCB);
    if (mWindow)
        mWindow->setPCB(PCB);
    endExclusiveTask();
}
void QPCBApplication::_setAutonomousAgent(const std::shared_ptr<Agent> &A)
{
    INFO("QPCBApplication setAutonomousAgent = " << A.get() << " \"" << (A ? A->name() : "") << '"');

    startExclusiveTask("You started a task while changing the UI's agent, UI will block.");
    {
    py::GIL_guard GIL; mAutoAgent = A;
    }
    if (mWindow)
        mWindow->getParameterTab()->addParameters(A->getParameters());
    endExclusiveTask();
}
bool QPCBEventReceiver::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::Show:
        if (!self.mWindow)
            self.mWindow = std::make_unique<Window>(self);
        if (self.mWindow)
            self.mWindow->show();
        break;
    case QEvent::Hide:
        if (self.mWindow)
            self.mWindow->hide(); // self.mWindow.reset()
        break;
    case QEvent::User:
        if (auto E = dynamic_cast<SetPCBEvent *>(event))
            self._setPCB(E->PCB, E->TaskActive);
        else if (auto E = dynamic_cast<SetAutoAgentEvent *>(event))
            self._setAutonomousAgent(E->agent);
        else if (auto E = dynamic_cast<TerminateEvent *>(event))
            QApplication::exit(E->exitCode);
        break;
    default:
        return QObject::event(event);
    }
    event->accept();
    return true;
}
