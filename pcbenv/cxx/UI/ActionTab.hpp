
#ifndef GYM_PCB_UI_ACTIONTAB_H
#define GYM_PCB_UI_ACTIONTAB_H

#include "Py.hpp"
#include <QWidget>
#include <set>

class Window;
class UIActions;
class Pin;
class Component;
class PCBoard;

class QCheckBox;
class QComboBox;
class QListWidget;
class QPushButton;
class QProgressBar;
class QSpinBox;
class QDoubleSpinBox;
class QToolButton;

// Actions:
// Autoroute Selection + Timeout
// Unroute Selection
// Autoroute Connection
// Route To Point + Preview
// Segment To Point + Preview
// None
// SetLayer
// Unroute

class ActionTab : public QWidget
{
    Q_OBJECT
public:
    bool init();
    void connect(Window&, UIActions&);
    uint getLayer() const;
    uint64_t getTimeoutMS() const;
    void setBoard(const PCBoard *);
    void setNumLayers(uint N);
    void setPins(const Pin *, const Pin *);
    void setProgress(uint);
    void setActiveTool(char);
    void newSelections();
private:
    UIActions *mActions{0};
    QCheckBox *mPreview;
    QCheckBox *mStepLock;
    QComboBox *mLayerChoice;
    QComboBox *mTimeoutUnit;
    QComboBox *mSelectType;
    QComboBox *mSelectZ;
    QListWidget *mSelection;
    QSpinBox *mTimeoutSpin;
    QDoubleSpinBox *mViaCost;
    QProgressBar *mProgress;
    QPushButton *mAutoroute;
    QPushButton *mConnect;
    QPushButton *mInterrupt;
    QPushButton *mUnrouteX;
    QPushButton *mUnrouteS;
    QPushButton *mExit;
    QToolButton *mTools[2];
    QToolButton *mUndoTool;
    QToolButton *mNextStep;
private:
    void onClickTool(char c, Window&);
};

#endif // GYM_PCB_UI_ACTIONTAB_H
