
#include "Log.hpp"
#include "Util/Util.hpp"
#include "UI/BrowserTab.hpp"
#include "UI/GLWidget.hpp"
#include "UI/Qt/Loader.hpp"
#include "PCBoard.hpp"
#include "Net.hpp"
#include "Component.hpp"
#include <QTreeView>

class BrowserItem
{
public:
    BrowserItem(const std::string &s);
    BrowserItem(const Object *);
    const QString& getText() const { return mText; }
    const Object *getObject() const { return mRef; }
    BrowserItem *getParent() const { return mParent; }
    BrowserItem *getChild(int index) const { return mChildren.value(index); }
    const QList<BrowserItem *>& getChildren() const { return mChildren; }
    void addChild(BrowserItem *A) { mChildren.append(A); A->mParent = this; }
    uint getIndex() const { return mParent ? mParent->mChildren.indexOf(const_cast<BrowserItem *>(this)) : 0; }
private:
    BrowserItem *mParent{0};
    QList<BrowserItem *> mChildren;
    const Object *const mRef;
    QString mText;
};
BrowserItem::BrowserItem(const std::string &s) : mRef(0)
{
    mText = QString::fromStdString(s);
}
BrowserItem::BrowserItem(const Object *A) : mRef(A)
{
    if (!mRef)
        return;
    auto P = mRef->as<const Pin>();
    mText = QString::fromStdString(mRef->name());
    if (P && P->hasNet()) {
        mText.append(" [");
        mText.append(P->net()->name().c_str());
        mText.append("]");
    }
}

void BrowserTab::init()
{
    auto W = LoadUIFile(this, "browser_tab.ui");
    if (!W)
        return;
    mTreeView = W->findChild<QTreeView *>("Tree");
    if (!mTreeView)
        return;
    QObject::connect(mTreeView, &QTreeView::doubleClicked, this, &BrowserTab::onClickItem);
}

void BrowserTab::onClickItem(const QModelIndex &index)
{
    auto A = static_cast<BrowserItem *>(index.internalPointer());
    if (!A)
        return;
    if (mWidget && A->getObject())
        mWidget->focusOn(*A->getObject());
}

class BrowserIM : public QAbstractItemModel
{
public:
    BrowserIM(BrowserItem &root, QObject *parent);
    QModelIndex parent(const QModelIndex&) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QVariant data(const QModelIndex&, int role) const override;
    QVariant headerData(int section, Qt::Orientation, int role) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
private:
    BrowserItem &mRoot;
};
BrowserIM::BrowserIM(BrowserItem &root, QObject *parent) : QAbstractItemModel(parent), mRoot(root)
{
}
QModelIndex BrowserIM::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();
    auto I = static_cast<BrowserItem *>(index.internalPointer());
    auto P = I->getParent();
    if (P == &mRoot)
        return QModelIndex();
    return createIndex(P->getIndex(), 0, P);
}
int BrowserIM::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;
    if (!parent.isValid())
        return mRoot.getChildren().count();
    return static_cast<BrowserItem *>(parent.internalPointer())->getChildren().count();
}
int BrowserIM::columnCount(const QModelIndex &parent) const
{
    return 1;
}
QModelIndex BrowserIM::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();
    auto P = parent.isValid() ? static_cast<BrowserItem *>(parent.internalPointer()) : &mRoot;
    auto I = P->getChild(row);
    if (!I)
        return QModelIndex();
    return createIndex(row, column, I);
}
QVariant BrowserIM::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole)
        return QVariant();
    return static_cast<BrowserItem *>(index.internalPointer())->getText();
}
QVariant BrowserIM::headerData(int section, Qt::Orientation orientation, int role) const
{
     if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section == 0)
         return "PCB Items";
     return QVariant();
}

namespace
{
template<typename T> std::list<T *> sortedBy_name(const std::vector<T *> &items)
{
    std::list<T *> L;
    for (auto I : items)
        L.push_back(I);
    L.sort([](const T *LHS, const T *RHS){ return util::lexiconumeric_compare(LHS->name(), RHS->name()); });
    return L;
}
}
void BrowserTab::setBoard(const PCBoard *PCB)
{
    auto root = std::make_unique<BrowserItem>(nullptr);
    auto nets = new BrowserItem("Nets");
    auto coms = new BrowserItem("Components");
    if (!root ||
        !nets ||
        !coms)
        return;
    root->addChild(coms);
    root->addChild(nets);
    for (auto C : sortedBy_name(PCB->getComponents())) {
        auto I = new BrowserItem(C);
        if (!I)
            break;
        coms->addChild(I);
        for (auto P : sortedBy_name(C->getPins())) {
            auto B = new BrowserItem(P);
            if (B)
                I->addChild(B);
        }
    }
    for (auto N : sortedBy_name(PCB->getNets())) {
        auto I = new BrowserItem(N->name());
        if (!I)
            break;
        nets->addChild(I);
        for (auto X : N->connections()) {
            if (!X->sourcePin() || !X->targetPin())
                continue;
            auto B = new BrowserItem(X->sourcePin()->getFullName() + " -> " + X->targetPin()->getFullName());
            if (B)
                I->addChild(B);
        }
    }
    auto IM = new BrowserIM(*root, 0);
    if (!IM)
        return;
    mTreeView->setModel(IM);
    root.release();
}
