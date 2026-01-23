#pragma once
#include <QWidget>
#include <QTreeWidget>
#include <memory>
#include "AppInterfaces.h"

class SceneTreePanel : public QWidget
{
    Q_OBJECT
public:
    explicit SceneTreePanel(QWidget* parent = nullptr);

    void setSession(const std::shared_ptr<AbstractDataManager>& dataMgr,
        const QString& sourcePath);

private:
    QTreeWidget* tree_ = nullptr;
    QTreeWidgetItem* root_ = nullptr;
};
