#ifndef GYM_PCB_UI_VIEWTAB_H
#define GYM_PCB_UI_VIEWTAB_H

#include "Py.hpp"
#include <QWidget>
#include <QHBoxLayout>
class QCheckBox;
class QLabel;
class QPushButton;
class QSlider;
class QVBoxLayout;

class GLWidget;
class ViewTab;

class LayerAlphaSlider : public QHBoxLayout
{
    Q_OBJECT
public:
    LayerAlphaSlider(ViewTab *, int layerIndex);
    ~LayerAlphaSlider();
    int getLayer() const { return mZ; }
    QSlider *getSlider() const { return mSlider; }
private:
    QSlider *mSlider;
    const int mZ;
};

class ViewTab : public QWidget
{
    Q_OBJECT
public:
    bool init(GLWidget *);
    void setNumLayers(GLWidget *, uint);
public slots:
    void onLayerAlphaSlider(int layerIndex, int value);
    void onSlowdownSlider(int);
    uint slowdownToSliderPosition(uint us) const;
    void onSave();
private:
    QPushButton *mZDirection;
    QPushButton *mScreenshot;
    QPushButton *mResetZoom;
    QCheckBox *mSelectionLayers;
    QCheckBox *mSelectionType;
    QCheckBox *mShowGridLines;
    QCheckBox *mShowGridPoints;
    QCheckBox *mShowRatsNest;
    QCheckBox *mLimitRatsNest;
    QCheckBox *mShowTriangles;
    QCheckBox *mShowAStar;
    QVBoxLayout *mAlphaSliders;
    QSlider *mSlowdown;
    QLabel *mSlowdownLabel;
private:
    GLWidget *WGL{0};
private:
    bool changeDirection();
};

#endif // GYM_PCB_UI_VIEWTAB_H
