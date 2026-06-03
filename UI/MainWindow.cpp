#include "MainWindow.h"

#include <QApplication>
#include <QToolBar>
#include <QPushButton>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QStatusBar>
#include <QDir>
#include <QFontDatabase>
#include <QSizePolicy>
#include <filesystem>

namespace fs = std::filesystem;

// Visual Studio 2022 Dark — global stylesheet
static const QString APP_STYLE = R"(
/* ── Base surfaces ──────────────────────────────────────────────────────── */
QMainWindow, QWidget {
    background: #1E1E1E;
    color: #D4D4D4;
    font-family: "Segoe UI", "Consolas", Arial, sans-serif;
    font-size: 12px;
}

/* ── Menu bar ────────────────────────────────────────────────────────────── */
QMenuBar {
    background: #2D2D30;
    color: #D4D4D4;
    border-bottom: 1px solid #3F3F46;
    padding: 2px 0;
    font-size: 12px;
}
QMenuBar::item {
    background: transparent;
    padding: 4px 12px;
}
QMenuBar::item:selected {
    background: #3F3F46;
    color: #FFFFFF;
}
QMenuBar::item:pressed {
    background: #007ACC;
    color: #FFFFFF;
}
QMenu {
    background: #1E1E1E;
    color: #D4D4D4;
    border: 1px solid #3F3F46;
    padding: 4px 0;
}
QMenu::item {
    padding: 5px 28px 5px 20px;
    background: transparent;
}
QMenu::item:selected {
    background: #094771;
    color: #FFFFFF;
}
QMenu::separator {
    height: 1px;
    background: #3F3F46;
    margin: 4px 0;
}

/* ── Toolbar ─────────────────────────────────────────────────────────────── */
QToolBar {
    background: #2D2D30;
    border-bottom: 1px solid #3F3F46;
    spacing: 4px;
    padding: 3px 6px;
}
QToolBar::separator {
    background: #3F3F46;
    width: 1px;
    margin: 4px 6px;
}

/* ── Dock widget ─────────────────────────────────────────────────────────── */
QDockWidget {
    border: 1px solid #3F3F46;
    titlebar-close-icon: url(none);
    font-size: 11px;
}
QDockWidget::title {
    background: #2D2D30;
    color: #D4D4D4;
    padding: 5px 8px;
    border-bottom: 2px solid #007ACC;
    font-weight: normal;
    text-align: left;
}
QDockWidget::close-button, QDockWidget::float-button {
    background: transparent;
    border: none;
    padding: 2px;
}

/* ── Status bar ──────────────────────────────────────────────────────────── */
QStatusBar {
    background: #007ACC;
    color: #FFFFFF;
    font-size: 11px;
    padding: 0 6px;
}
QStatusBar::item { border: none; }

/* ── Scroll bars ─────────────────────────────────────────────────────────── */
QScrollBar:vertical {
    background: #1E1E1E;
    width: 12px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: #424242;
    min-height: 20px;
    border-radius: 2px;
    margin: 2px;
}
QScrollBar::handle:vertical:hover { background: #686868; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal {
    background: #1E1E1E;
    height: 12px;
    margin: 0;
}
QScrollBar::handle:horizontal {
    background: #424242;
    min-width: 20px;
    border-radius: 2px;
    margin: 2px;
}
QScrollBar::handle:horizontal:hover { background: #686868; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

/* ── Splitter ────────────────────────────────────────────────────────────── */
QSplitter::handle {
    background: #3F3F46;
    width: 1px;
    height: 1px;
}

/* ── Tooltip ─────────────────────────────────────────────────────────────── */
QToolTip {
    background: #252526;
    color: #D4D4D4;
    border: 1px solid #007ACC;
    padding: 4px 6px;
    font-size: 11px;
}

/* ── Buttons ─────────────────────────────────────────────────────────────── */
QPushButton#openBtn {
    background: #007ACC;
    color: #FFFFFF;
    border: none;
    padding: 5px 16px;
    font-size: 12px;
    font-weight: 600;
    border-radius: 2px;
    min-height: 22px;
}
QPushButton#openBtn:hover  { background: #1C97EA; }
QPushButton#openBtn:pressed { background: #005F9E; }

QPushButton#navBtn {
    background: #3F3F46;
    color: #D4D4D4;
    border: 1px solid #555558;
    padding: 4px 12px;
    font-size: 12px;
    border-radius: 2px;
    min-height: 22px;
}
QPushButton#navBtn:hover   { background: #505055; color: #FFFFFF; }
QPushButton#navBtn:pressed { background: #2D2D30; }
QPushButton#navBtn:checked {
    background: #094771;
    color: #FFFFFF;
    border: 1px solid #007ACC;
}

/* ── Labels ──────────────────────────────────────────────────────────────── */
QLabel#pathLabel {
    color: #9D9D9D;
    font-size: 11px;
    padding: 0 8px;
}
QLabel#badge {
    color: #007ACC;
    font-size: 11px;
    font-weight: bold;
    padding: 0 8px;
}

/* ── Scroll area ─────────────────────────────────────────────────────────── */
QScrollArea { border: none; background: #1E1E1E; }
)";


// =============================================================================
//  Constructor
// =============================================================================
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Matrix One — Influence Line Viewer");
    setMinimumSize(1100, 700);
    resize(1280, 800);
    setAcceptDrops(true);
    setStyleSheet(APP_STYLE);

    buildToolbar();
    buildDock();
    buildStack();
    setStatus("Drop a project folder or click 'Open Project'");
}

// =============================================================================
//  buildToolbar
// =============================================================================
void MainWindow::buildToolbar()
{
    // ── Menu bar (VS-style) ────────────────────────────────────────────────
    auto* mb = menuBar();
    mb->setNativeMenuBar(false);

    auto* fileMenu = mb->addMenu("File");
    fileMenu->addAction("Open Project...", this, &MainWindow::onOpenProject,
                        QKeySequence("Ctrl+Shift+O"));
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", qApp, &QApplication::quit,
                        QKeySequence("Alt+F4"));

    auto* viewMenu = mb->addMenu("View");
    viewMenu->addAction("Project Browser", this, [this]() {
        dockWidget_->setVisible(!dockWidget_->isVisible());
    });
    viewMenu->addSeparator();
    viewMenu->addAction("Beam View",          this, [this](){ stack_->setCurrentIndex(6); });
    viewMenu->addAction("Influence Lines",    this, [this](){ stack_->setCurrentIndex(1); });
    viewMenu->addAction("Critical Values",    this, [this](){ stack_->setCurrentIndex(2); });
    viewMenu->addAction("Load Envelopes",     this, [this](){ stack_->setCurrentIndex(3); });
    viewMenu->addAction("Model Information",  this, [this](){ stack_->setCurrentIndex(5); });

    auto* helpMenu = mb->addMenu("Help");
    helpMenu->addAction("About Matrix One", this, [this]() {
        setStatus("Matrix One — Influence Line Viewer  v1.0  |  © 2025 Matrix One");
    });

    // ── Toolbar ────────────────────────────────────────────────────────────
    auto* bar = addToolBar("Main");
    bar->setMovable(false);
    bar->setFloatable(false);
    bar->setIconSize(QSize(16, 16));

    // Open Project button (VS blue)
    auto* openBtn = new QPushButton("📂  Open Project");
    openBtn->setObjectName("openBtn");
    openBtn->setToolTip("Open a Matrix One project folder  (Ctrl+Shift+O)");
    connect(openBtn, &QPushButton::clicked, this, &MainWindow::onOpenProject);
    bar->addWidget(openBtn);

    bar->addSeparator();

    // Navigation buttons (VS-style flat buttons)
    auto makeNav = [&](const QString& icon, const QString& label,
                       const QString& tip, int idx) {
        auto* btn = new QPushButton(icon + "  " + label);
        btn->setObjectName("navBtn");
        btn->setToolTip(tip);
        btn->setCheckable(true);
        connect(btn, &QPushButton::clicked, this, [this, btn, idx]() {
            // Uncheck other nav buttons
            for (auto* b : findChildren<QPushButton*>()) {
                if (b->objectName() == "navBtn" && b != btn)
                    b->setChecked(false);
            }
            btn->setChecked(true);
            stack_->setCurrentIndex(idx);
        });
        bar->addWidget(btn);
        return btn;
    };

    makeNav("🏗",  "Beam",         "Beam Visualization",     6);
    makeNav("📈",  "Inf. Lines",   "Influence Line Viewer",  1);
    makeNav("⚡",  "Critical",     "Critical Values",        2);
    makeNav("📊",  "Envelopes",    "Load Envelopes",         3);
    makeNav("🔧",  "Model Info",   "Structural Model Info",  5);

    bar->addSeparator();

    // Path label
    pathLabel_ = new QLabel("No project loaded");
    pathLabel_->setObjectName("pathLabel");
    bar->addWidget(pathLabel_);

    // Spacer
    auto* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    bar->addWidget(spacer);

    // App badge
    auto* badge = new QLabel("Matrix One");
    badge->setObjectName("badge");
    bar->addWidget(badge);
}


// =============================================================================
//  buildDock
// =============================================================================
void MainWindow::buildDock()
{
    browser_ = new ProjectBrowser();
    browser_->setMinimumWidth(220);
    browser_->setMaximumWidth(340);

    dockWidget_ = new QDockWidget("Solution Explorer", this);
    dockWidget_->setObjectName("projectDock");
    dockWidget_->setWidget(browser_);
    dockWidget_->setFeatures(QDockWidget::DockWidgetMovable
                           | QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::LeftDockWidgetArea, dockWidget_);

    connect(browser_, &ProjectBrowser::curveSelected,
            this, &MainWindow::onCurveSelected);
    connect(browser_, &ProjectBrowser::criticalValuesRequested,
            this, &MainWindow::onCriticalValuesRequested);
    connect(browser_, &ProjectBrowser::envelopeRequested,
            this, &MainWindow::onEnvelopeRequested);
    connect(browser_, &ProjectBrowser::imageDirSelected,
            this, &MainWindow::onImageDirSelected);
    connect(browser_, &ProjectBrowser::modelInfoRequested,
            this, &MainWindow::onModelInfoRequested);
}


// =============================================================================
//  buildStack — Welcome page + all content panels
// =============================================================================
void MainWindow::buildStack()
{
    stack_ = new QStackedWidget();
    setCentralWidget(stack_);

    // ── 0 : Welcome ──────────────────────────────────────────────────────────
    welcome_ = new QWidget();
    welcome_->setStyleSheet("background:#1E1E1E;");
    auto* wl = new QVBoxLayout(welcome_);
    wl->setAlignment(Qt::AlignCenter);

    // Top accent bar
    auto* accentBar = new QFrame();
    accentBar->setFixedHeight(3);
    accentBar->setStyleSheet("background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                             "stop:0 #007ACC, stop:0.5 #1C97EA, stop:1 #007ACC);"
                             "border:none;");

    auto* icon = new QLabel("🏗");
    icon->setStyleSheet("font-size:56px; color:#007ACC;");
    icon->setAlignment(Qt::AlignCenter);

    auto* title = new QLabel("Matrix One");
    title->setStyleSheet(
        "font-size:26px; font-weight:700; color:#FFFFFF;"
        "letter-spacing:2px; margin-top:16px; font-family:'Segoe UI';");
    title->setAlignment(Qt::AlignCenter);

    auto* subtitle = new QLabel("Influence Line Viewer");
    subtitle->setStyleSheet(
        "font-size:13px; color:#007ACC; font-family:'Segoe UI';"
        "letter-spacing:1px; margin-bottom:8px;");
    subtitle->setAlignment(Qt::AlignCenter);

    auto* sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color:#3F3F46; margin:12px 80px;");

    auto* sub = new QLabel(
        "Open a project folder containing  path.json\n"
        "or drag-and-drop it onto this window.");
    sub->setStyleSheet(
        "font-size:12px; color:#9D9D9D;"
        "margin-top:8px; font-family:'Segoe UI';");
    sub->setAlignment(Qt::AlignCenter);

    auto* openBtn2 = new QPushButton("📂  Open Project");
    openBtn2->setObjectName("openBtn");
    openBtn2->setFixedWidth(200);
    openBtn2->setToolTip("Open a Matrix One project folder");
    connect(openBtn2, &QPushButton::clicked, this, &MainWindow::onOpenProject);

    auto* hint = new QLabel("Keyboard shortcut: Ctrl+Shift+O");
    hint->setStyleSheet("font-size:10px; color:#555558;");
    hint->setAlignment(Qt::AlignCenter);

    wl->addWidget(accentBar);
    wl->addStretch(1);
    wl->addWidget(icon);
    wl->addWidget(title);
    wl->addWidget(subtitle);
    wl->addWidget(sep);
    wl->addWidget(sub);
    wl->addSpacing(20);
    wl->addWidget(openBtn2, 0, Qt::AlignCenter);
    wl->addSpacing(6);
    wl->addWidget(hint, 0, Qt::AlignCenter);
    wl->addStretch(2);


    stack_->addWidget(welcome_);    // index 0

    // ── 1 : Influence Line Viewer ─────────────────────────────────────────────
    ilViewer_ = new InfluenceLineViewer();
    stack_->addWidget(ilViewer_);   // index 1

    // ── 2 : Critical Values ───────────────────────────────────────────────────
    critPanel_ = new CriticalValuesPanel();
    stack_->addWidget(critPanel_);  // index 2

    // ── 3 : Load Envelopes ────────────────────────────────────────────────────
    envPanel_ = new LoadEnvelopePanel();
    stack_->addWidget(envPanel_);   // index 3

    // ── 4 : Image Gallery ─────────────────────────────────────────────────────
    gallery_ = new ImageGallery();
    stack_->addWidget(gallery_);    // index 4

    // ── 5 : Model Info ────────────────────────────────────────────────────────
    modelPanel_ = new ModelInfoPanel();
    stack_->addWidget(modelPanel_); // index 5

    // ── 6 : Beam Viewer ───────────────────────────────────────────────────────
    beamPanel_ = new BeamPanel();
    stack_->addWidget(beamPanel_);  // index 6

    stack_->setCurrentIndex(0);
}

// =============================================================================
//  onOpenProject
// =============================================================================
void MainWindow::onOpenProject()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        "Open Matrix One Project",
        QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty())
        loadProject(dir);
}

// =============================================================================
//  loadProject
// =============================================================================
void MainWindow::loadProject(const QString& dir)
{
    jutils::Paths p(fs::path(dir.toStdString()));

    if (!p.valid()) {
        setStatus(QString("Invalid project: %1").arg(dir), true);
        return;
    }

    paths_ = p;

    // Also try to resolve configPath from path.json if one exists
    const auto resolved = jutils::detect_root(p.root);
    if (!resolved.empty() && fs::exists(resolved))
        paths_ = jutils::Paths(resolved);

    browser_->setRoot(paths_);
    ilViewer_->setPaths(paths_);
    critPanel_->setPaths(paths_);
    envPanel_->setPaths(paths_);
    modelPanel_->setPaths(paths_);
    beamPanel_->setPaths(paths_);

    pathLabel_->setText(QString::fromStdString(paths_.root.string()));
    setStatus(QString("Project loaded: %1")
              .arg(QString::fromStdString(paths_.root.filename().string())));
    stack_->setCurrentIndex(0); // Show welcome until user picks something
}

// =============================================================================
//  Slots from ProjectBrowser
// =============================================================================
void MainWindow::onCurveSelected(const QString& /*name*/, const QString& file)
{
    ilViewer_->showCurve(file);
    stack_->setCurrentIndex(1);
    // Sync le beam viewer avec la meme courbe
    for (int i = 0; i < 4; ++i) {
        if (QString::fromLatin1(jutils::curve_files()[i]) == file) {
            beamPanel_->syncFromViewer(i, 0, 0, -1.0);
            break;
        }
    }
}

void MainWindow::onCriticalValuesRequested()
{
    stack_->setCurrentIndex(2);
}

void MainWindow::onEnvelopeRequested(const QString& scope, const QString& loadType)
{
    envPanel_->showEnvelope(scope, loadType);
    stack_->setCurrentIndex(3);
}

void MainWindow::onImageDirSelected(const QString& dirPath)
{
    gallery_->setDirectory(dirPath);
    stack_->setCurrentIndex(4);
}

void MainWindow::onModelInfoRequested()
{
    stack_->setCurrentIndex(5);
}

// =============================================================================
//  Drag & Drop support — drop a project folder directly onto the window
// =============================================================================
void MainWindow::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasUrls())
        e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* e)
{
    const auto& urls = e->mimeData()->urls();
    if (!urls.isEmpty()) {
        const QString path = urls.first().toLocalFile();
        if (QDir(path).exists())
            loadProject(path);
    }
}

// =============================================================================
//  Status bar
// =============================================================================
void MainWindow::setStatus(const QString& msg, bool error)
{
    // VS: status bar is always blue; errors shown in red text
    statusBar()->setStyleSheet(
        error ? "background:#007ACC; color:#F44747; font-weight:bold;"
              : "background:#007ACC; color:#FFFFFF;");
    statusBar()->showMessage("  " + msg);
}

