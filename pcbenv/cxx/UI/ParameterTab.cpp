
#include "Log.hpp"
#include "UserSettings.hpp"
#include "UI/ParameterTab.hpp"
#include "Parameter.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>

class ParameterWidget : public QWidget
{
public:
    ParameterWidget(Parameter&);
    static ParameterWidget *create(Parameter&);
    virtual void update() = 0;
protected:
    Parameter &mParam;
    QVBoxLayout *mLayout;
};
class DParameterWidget : public ParameterWidget
{
public:
    DParameterWidget(Parameter&);
    void update() override;
private:
    QDoubleSpinBox *mValue;
};
class IParameterWidget : public ParameterWidget
{
public:
    IParameterWidget(Parameter&);
    void update() override;
private:
    QSpinBox *mValue;
};
class EParameterWidget : public ParameterWidget
{
public:
    EParameterWidget(Parameter&);
    void update() override;
private:
    QComboBox *mValue;
};
class BParameterWidget : public ParameterWidget
{
public:
    BParameterWidget(Parameter&);
    void update() override;
private:
    QCheckBox *mValue;
};

ParameterWidget::ParameterWidget(Parameter &param) : mParam(param)
{
    mLayout = new QVBoxLayout(this);
    if (!mLayout)
        return;
    mLayout->addWidget(new QLabel(param.name().c_str()));
}
DParameterWidget::DParameterWidget(Parameter &param) : ParameterWidget(param)
{
    mValue = new QDoubleSpinBox;
    if (!mValue)
        return;
    mValue->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
    mLayout->addWidget(mValue);
    update();
    QObject::connect(mValue, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double v){ mParam.set(v); });
}
IParameterWidget::IParameterWidget(Parameter &param) : ParameterWidget(param)
{
    mValue = new QSpinBox;
    if (!mValue)
        return;
    mValue->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
    mLayout->addWidget(mValue);
    update();
    QObject::connect(mValue, QOverload<int>::of(&QSpinBox::valueChanged), [this](int n){ mParam.set(int64_t(n)); });
}
EParameterWidget::EParameterWidget(Parameter &param) : ParameterWidget(param)
{
    mValue = new QComboBox;
    if (!mValue)
        return;
    mLayout->addWidget(mValue);
    for (const auto &v : param.getEnum())
        mValue->addItem(v.c_str());
    update();
    QObject::connect(mValue, &QComboBox::currentTextChanged, [this](const QString &text){ mParam.set(text.toStdString()); });
}
BParameterWidget::BParameterWidget(Parameter &param) : ParameterWidget(param)
{
    mValue = new QCheckBox;
    if (!mValue)
        return;
    mLayout->addWidget(mValue);
    update();
    QObject::connect(mValue, &QCheckBox::clicked, [this](bool b){ mParam.set(b); });
}

void DParameterWidget::update()
{
    const double d = mParam.d();
    mValue->setRange(mParam.min(), mParam.max());
    mValue->setDecimals(std::max(1, 3 - int(std::log10(std::abs(d)))));
    mValue->setValue(d);
}
void IParameterWidget::update()
{
    mValue->setRange(mParam.min(), mParam.max());
    mValue->setValue(mParam.i());
}
void EParameterWidget::update()
{
    mValue->setCurrentText(mParam.s().c_str());
}
void BParameterWidget::update()
{
    mValue->setChecked(mParam.b());
}

ParameterWidget *ParameterWidget::create(Parameter &param)
{
    switch (param.dtype()) {
    default:
        break;
    case 'd': return new DParameterWidget(param);
    case 'i': return new IParameterWidget(param);
    case 'b': return new BParameterWidget(param);
    case 's':
        if (!param.getEnum().empty())
            return new EParameterWidget(param);
        return 0;
    }
    return 0;
}

bool ParameterTab::init()
{
    mScroll = new QScrollArea;
    if (!mScroll)
        return false;
    mScroll->setWidgetResizable(true);
    mScroll->setGeometry(this->rect());

    auto main = new QVBoxLayout(this);
    if (!main)
        return false;
    main->addWidget(mScroll);

    auto list = new QWidget();
    if (!list)
        return false;
    mLayout = new QVBoxLayout(list);

    mScroll->setWidget(list);
    return true;
}

void ParameterTab::addParameters(const std::map<std::string, Parameter *> &params)
{
    clear();
    for (const auto &I : params) {
        if (!I.second->visible())
            continue;
        auto W = ParameterWidget::create(*I.second);
        if (W)
            mLayout->addWidget(W);
    }
    mLayout->addStretch();
}

void ParameterTab::clear()
{
    QLayoutItem *item;
    while ((item = mLayout->takeAt(0)) != 0)
        delete item->widget();
}
