#pragma once
#include <QWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include "JsonUtils.h"

// =============================================================================
//  ProjectBrowser — Explorateur de l'arborescence du projet
// =============================================================================
//  Signals emitted when user clicks on a tree item :
//
//    curveSelected(curveName, curveFile)
//        → user picked an influence line curve
//    criticalValuesRequested()
//        → user clicked the "Critical Values" node
//    envelopeRequested(scope, loadType)
//        → user clicked a load envelope node (scope = Global/Critical_Section)
//    imageDirSelected(dirPath)
//        → user clicked a Plots sub-folder
//    modelInfoRequested()
//        → user clicked Structural Model
//
class ProjectBrowser : public QWidget
{
    Q_OBJECT
public:
    explicit ProjectBrowser(QWidget* parent = nullptr);

    void setRoot(const jutils::Paths& p);
    void clear();

signals:
    void curveSelected(const QString& curveName, const QString& curveFile);
    void criticalValuesRequested();
    void envelopeRequested(const QString& scope, const QString& loadType);
    void imageDirSelected(const QString& dirPath);
    void modelInfoRequested();

private slots:
    void onItemClicked(QTreeWidgetItem* item, int col);

private:
    void buildTree();
    QTreeWidgetItem* addGroup(QTreeWidgetItem* parent,
                              const QString& label,
                              const QString& iconName = "");

    QTreeWidget*  tree_;
    jutils::Paths paths_;
};
