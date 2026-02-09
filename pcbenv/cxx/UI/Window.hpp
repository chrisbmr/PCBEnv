
#ifndef GYM_PCB_UI_WINDOW_H
#define GYM_PCB_UI_WINDOW_H

#include "GLWidget.hpp"
#include <QWidget>
#include <QEvent>

class QTimer;
class QLabel;
class QTabWidget;
class ActionTab;
class BrowserTab;
class ViewTab;
class ParameterTab;
class PCBoard;
class UIApplication;
class UIActions;

class Window : public QWidget
{
    Q_OBJECT

public:
    Window(UIApplication&);
    ~Window();

    ActionTab *getActionTab() const { return mActionTab; }
    ParameterTab *getParameterTab() const { return mParamTab; }

    GLWidget *getGLWidget() const { return GLWidget::getInstance(); }

    void setPCB(const std::shared_ptr<PCBoard>&);

    UIApplication *getApplication() const { return &mUIA; }

    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;

public slots:
    void terminate();

private:
    UIApplication &mUIA;
    std::unique_ptr<UIActions> mActions;
    std::unique_ptr<QTabWidget> mTabContainer;
    ActionTab *mActionTab;
    BrowserTab *mBrowserTab;
    ViewTab *mViewTab;
    ParameterTab *mParamTab;
    std::unique_ptr<QTimer> mTimer;
};

inline void Window::terminate()
{
    std::exit(0);
}

#endif // GYM_PCB_UI_WINDOW_H
