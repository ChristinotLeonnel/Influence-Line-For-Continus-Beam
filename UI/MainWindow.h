#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QDockWidget>

#include "ProjectBrowser.h"
#include "InfluenceLineViewer.h"
#include "CriticalValuesPanel.h"
#include "LoadEnvelopePanel.h"
#include "ImageGallery.h"
#include "ModelInfoPanel.h"
#include "BeamPanel.h"
#include "JsonUtils.h"

// =============================================================================
//  MainWindow
//  Layout :
//    ┌────────────────────────────────────────────────────────────┐
//    │  Toolbar: [Open Project]  [path label]          [dark bg] │
//    ├──────────────┬─────────────────────────────────────────────┤
//    │  Project     │                                             │
//    │  Browser     │         Content Stack                       │
//    │  (QDock)     │  (Welcome | InfluenceLine | Critical |      │
//    │              │   Envelope | Gallery | Model)               │
//    └──────────────┴─────────────────────────────────────────────┘
//    └──── Status bar ────────────────────────────────────────────┘
// =============================================================================
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

public slots:
    void loadProject(const QString& dir);

private slots:
    void onOpenProject();

    // Browser signals
    void onCurveSelected(const QString& name, const QString& file);
    void onCriticalValuesRequested();
    void onEnvelopeRequested(const QString& scope, const QString& loadType);
    void onImageDirSelected(const QString& dirPath);
    void onModelInfoRequested();

private:
    void buildToolbar();
    void buildDock();
    void buildStack();
    void setStatus(const QString& msg, bool error = false);

    // ── Toolbar widgets ───────────────────────────────────────────────────────
    QLabel*       pathLabel_   = nullptr;

    // ── Dock ─────────────────────────────────────────────────────────────────
    QDockWidget*   dockWidget_ = nullptr;
    ProjectBrowser* browser_   = nullptr;

    // ── Content stack ─────────────────────────────────────────────────────────
    QStackedWidget*     stack_      = nullptr;
    QWidget*            welcome_    = nullptr;   // index 0
    InfluenceLineViewer* ilViewer_  = nullptr;   // index 1
    CriticalValuesPanel* critPanel_ = nullptr;   // index 2
    LoadEnvelopePanel*   envPanel_  = nullptr;   // index 3
    ImageGallery*        gallery_   = nullptr;   // index 4
    ModelInfoPanel*      modelPanel_= nullptr;   // index 5
    BeamPanel*           beamPanel_ = nullptr;   // index 6

    jutils::Paths paths_;
};
