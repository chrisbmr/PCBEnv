
#ifndef GYM_PCB_UI_BROWSERTAB_H
#define GYM_PCB_UI_BROWSERTAB_H

#include "Py.hpp"
#include <QWidget>
class QTreeView;

class GLWidget;
class PCBoard;

class BrowserTab : public QWidget
{
    Q_OBJECT
public:
    void connect(GLWidget &GLW) { mWidget = &GLW; }
    void init();
    void setBoard(const PCBoard *);
private slots:
    void onClickItem(const QModelIndex&);
private:
    QTreeView *mTreeView;
    GLWidget *mWidget{0};
};

#endif // GYM_PCB_UI_BROWSERTAB_H
