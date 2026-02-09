#ifndef GYM_PCB_UI_PARAMETERTAB_H
#define GYM_PCB_UI_PARAMETERTAB_H

#include "Py.hpp"
#include <QWidget>
#include <map>
#include <string>

class QScrollArea;
class QVBoxLayout;

class Parameter;
class GLWidget;

class ParameterTab : public QWidget
{
    Q_OBJECT
public:
    bool init();
    void addParameters(const std::map<std::string, Parameter *>&);
    void clear();
private:
    QScrollArea *mScroll;
    QVBoxLayout *mLayout;
};

#endif // GYM_PCB_UI_PARAMETERTAB_H
