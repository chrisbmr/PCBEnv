
#ifndef GYM_PCB_UI_QAPP_H
#define GYM_PCB_UI_QAPP_H

#include "Py.hpp"
#include "UI/Application.hpp"
#include <QObject>
#include <future>

class QPCBApplication;

class QPCBEventReceiver : public QObject
{
    Q_OBJECT
public:
    QPCBEventReceiver(QPCBApplication &A) : self(A) { }
    bool event(QEvent *) override;
private:
    QPCBApplication &self;
};

/// This encapsulates the Qt application.
/// We cannot inherit from QObject because we cannot create QObjects before a QApplication,
/// and we cannot use Q_OBJECT while inheriting from UIApplication.
class QPCBApplication : public UIApplication
{
    friend class QPCBEventReceiver;
public:
    QPCBApplication();
    ~QPCBApplication();
    std::future<bool> runAsync() override;
    void show() override;
    void hide() override;
    void quit(int code) override;
    void setAutonomousAgent(const std::shared_ptr<Agent>&, const std::shared_ptr<PCBoard>&) override;
    void setPCB(const std::shared_ptr<PCBoard>&, bool wait) override;
private:
    std::thread mThread;
    int argc{1};
    char *argv[1];
    void proc();

    std::unique_ptr<Window> mWindow;
    std::unique_ptr<QPCBEventReceiver> mEventReceiver;
    std::promise<bool> *mPromise;

    void _setPCB(const std::shared_ptr<PCBoard>&, bool lockedAlready);
    void _setAutonomousAgent(const std::shared_ptr<Agent>&);
};

#endif // GYM_PCB_UI_QAPP_H
