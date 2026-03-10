#include "SceneTreePanel.h"
#include <QVBoxLayout>
#include <vtkImageData.h>

SceneTreePanel::SceneTreePanel(QWidget* parent)
    : QWidget(parent)
{
    auto v = new QVBoxLayout(this);
    v->setContentsMargins(4, 4, 4, 4);

    tree_ = new QTreeWidget(this);
    tree_->setHeaderHidden(true);
    tree_->setStyleSheet("QTreeWidget{background:#1f1f1f; border:1px solid #333;}");

    root_ = new QTreeWidgetItem(tree_, QStringList() << QStringLiteral("场景"));
    root_->setExpanded(true);

    v->addWidget(tree_);
}

void SceneTreePanel::setSession(const std::shared_ptr<AbstractDataManager>& dataMgr,
    const QString& sourcePath)
{
    if (!root_) return;

    root_->takeChildren();

    if (!dataMgr || !dataMgr->GetVtkImage()) {
        new QTreeWidgetItem(root_, QStringList() << QStringLiteral("（未加载数据）"));
        root_->setExpanded(true); 
        return;
    }

    int dims[3] = { 0,0,0 };
    dataMgr->GetVtkImage()->GetDimensions(dims);

    const QString name = QStringLiteral("体素1: %1x%2x%3  (%4)")
        .arg(dims[0]).arg(dims[1]).arg(dims[2]).arg(sourcePath);

    auto* volItem = new QTreeWidgetItem(root_, QStringList() << name);
    volItem->setExpanded(true);

    // 预留
    new QTreeWidgetItem(root_, QStringList() << QStringLiteral("光源 1"));
    new QTreeWidgetItem(root_, QStringList() << QStringLiteral("光源 2"));

    root_->setExpanded(true);
    tree_->expandAll();
}
