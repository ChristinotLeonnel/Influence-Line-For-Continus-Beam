#include "ProjectBrowser.h"
#include <QHeaderView>
#include <QFont>
#include <filesystem>
namespace fs = std::filesystem;

// =============================================================================
//  Roles for tree items
// =============================================================================
enum ItemRole {
    RoleType   = Qt::UserRole,
    RoleData1  = Qt::UserRole + 1,
    RoleData2  = Qt::UserRole + 2,
    RoleData3  = Qt::UserRole + 3,
};

enum ItemType {
    TypeGroup            = 0,
    TypeModelInfo        = 1,
    TypeInfluenceLine    = 2,
    TypeCriticalValues   = 3,
    TypeEnvelope         = 4,
    TypeImageDir         = 5,
};

// =============================================================================
//  Constructor
// =============================================================================
ProjectBrowser::ProjectBrowser(QWidget* parent)
    : QWidget(parent)
{
    tree_ = new QTreeWidget(this);
    tree_->setHeaderHidden(true);
    tree_->setRootIsDecorated(true);
    tree_->setAnimated(true);
    tree_->setIndentation(16);
    tree_->setStyleSheet(
        "QTreeWidget { background: #1e2230; color: #cdd6f4; border: none; }"
        "QTreeWidget::item { padding: 4px 2px; }"
        "QTreeWidget::item:hover { background: #313244; }"
        "QTreeWidget::item:selected { background: #45475a; color: #cba6f7; }"
        "QTreeWidget::branch { background: #1e2230; }"
    );

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(tree_);

    connect(tree_, &QTreeWidget::itemClicked,
            this, &ProjectBrowser::onItemClicked);
}

// =============================================================================
//  setRoot — rebuild tree from project root
// =============================================================================
void ProjectBrowser::setRoot(const jutils::Paths& p)
{
    paths_ = p;
    buildTree();
}

void ProjectBrowser::clear()
{
    tree_->clear();
    paths_ = {};
}

// =============================================================================
//  buildTree
// =============================================================================
QTreeWidgetItem* ProjectBrowser::addGroup(QTreeWidgetItem* parent,
                                           const QString& label,
                                           const QString& /*iconName*/)
{
    auto* item = parent
        ? new QTreeWidgetItem(parent, QStringList{label})
        : new QTreeWidgetItem(tree_,  QStringList{label});
    QFont f = item->font(0);
    f.setBold(!parent);
    item->setFont(0, f);
    item->setData(0, RoleType, TypeGroup);
    return item;
}

void ProjectBrowser::buildTree()
{
    tree_->clear();
    if (!paths_.valid()) return;

    // ── Root label ────────────────────────────────────────────────────────────
    auto* root = new QTreeWidgetItem(tree_, QStringList{
        QString("📂  %1").arg(
            QString::fromStdString(paths_.root.filename().string()))
    });
    QFont rf = root->font(0);
    rf.setBold(true);
    rf.setPointSize(rf.pointSize() + 1);
    root->setFont(0, rf);
    root->setForeground(0, QColor("#cba6f7"));
    root->setData(0, RoleType, TypeGroup);
    root->setExpanded(true);

    // ── Structural Model ──────────────────────────────────────────────────────
    {
        auto* itm = new QTreeWidgetItem(root, QStringList{"🔧  Structural Model"});
        itm->setData(0, RoleType, TypeModelInfo);
    }

    // ── Influence Lines ───────────────────────────────────────────────────────
    auto* il = addGroup(root, "📊  Influence Lines");
    il->setExpanded(true);
    for (int i = 0; i < 4; ++i) {
        const QString lbl  = QString::fromLatin1(jutils::curve_labels()[i]);
        const QString file = QString::fromLatin1(jutils::curve_files()[i]);
        const auto    path = paths_.il_file(file.toStdString());

        auto* c = new QTreeWidgetItem(il, QStringList{
            QString("   %1").arg(lbl)
        });
        if (!fs::exists(path))
            c->setForeground(0, QColor("#6c7086"));   // greyed if not computed
        c->setData(0, RoleType,  TypeInfluenceLine);
        c->setData(0, RoleData1, lbl);
        c->setData(0, RoleData2, file);
    }

    // ── Critical Values ───────────────────────────────────────────────────────
    auto* cv = addGroup(root, "⚡  Critical Values");
    {
        auto* itm = new QTreeWidgetItem(cv, QStringList{"   All curves"});
        itm->setData(0, RoleType, TypeCriticalValues);
        cv->setExpanded(true);
    }

    // ── Load Envelopes ────────────────────────────────────────────────────────
    auto* env = addGroup(root, "📋  Load Envelopes");
    for (const auto* scope : {"Global", "Critical_Section"}) {
        auto* sgrp = new QTreeWidgetItem(env, QStringList{
            QString("   %1").arg(scope)
        });
        sgrp->setData(0, RoleType, TypeGroup);
        for (int i = 0; i < 3; ++i) {
            const QString lt    = QString::fromLatin1(jutils::load_type_dirs()[i]);
            const QString ltlbl = QString::fromLatin1(jutils::load_type_labels()[i]);
            const auto path = paths_.load_envelopes() / scope / lt.toStdString();
            auto* itm = new QTreeWidgetItem(sgrp, QStringList{
                QString("      %1").arg(ltlbl)
            });
            if (!fs::exists(path))
                itm->setForeground(0, QColor("#6c7086"));
            itm->setData(0, RoleType,  TypeEnvelope);
            itm->setData(0, RoleData1, QString(scope));
            itm->setData(0, RoleData2, lt);
        }
        sgrp->setExpanded(true);
    }
    env->setExpanded(false);

    // ── Plots ─────────────────────────────────────────────────────────────────
    auto* plots = addGroup(root, "🖼  Plots (06_Plots)");
    for (const auto* sub : {"All", "Maximum"}) {
        const auto d = paths_.plots() / sub;
        auto* itm = new QTreeWidgetItem(plots, QStringList{
            QString("   %1").arg(sub)
        });
        if (!fs::exists(d)) itm->setForeground(0, QColor("#6c7086"));
        itm->setData(0, RoleType,  TypeImageDir);
        itm->setData(0, RoleData1, QString::fromStdString(d.string()));
    }
    // Envelopes sub-dirs
    auto* envPlots = new QTreeWidgetItem(plots, QStringList{"   Envelopes"});
    envPlots->setData(0, RoleType, TypeGroup);
    for (int i = 0; i < 3; ++i) {
        const QString lt = QString::fromLatin1(jutils::load_type_dirs()[i]);
        const auto d = paths_.plots() / "Envelopes" / lt.toStdString();
        auto* itm = new QTreeWidgetItem(envPlots, QStringList{
            QString("      %1").arg(
                QString::fromLatin1(jutils::load_type_labels()[i]))
        });
        if (!fs::exists(d)) itm->setForeground(0, QColor("#6c7086"));
        itm->setData(0, RoleType,  TypeImageDir);
        itm->setData(0, RoleData1, QString::fromStdString(d.string()));
    }
    plots->setExpanded(false);
}

// =============================================================================
//  onItemClicked
// =============================================================================
void ProjectBrowser::onItemClicked(QTreeWidgetItem* item, int)
{
    if (!item) return;
    const int type = item->data(0, RoleType).toInt();

    switch (type) {
    case TypeModelInfo:
        emit modelInfoRequested();
        break;
    case TypeInfluenceLine:
        emit curveSelected(item->data(0, RoleData1).toString(),
                           item->data(0, RoleData2).toString());
        break;
    case TypeCriticalValues:
        emit criticalValuesRequested();
        break;
    case TypeEnvelope:
        emit envelopeRequested(item->data(0, RoleData1).toString(),
                               item->data(0, RoleData2).toString());
        break;
    case TypeImageDir:
        emit imageDirSelected(item->data(0, RoleData1).toString());
        break;
    default:
        break;
    }
}
