#include "ImageGallery.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QPixmap>
#include <QPushButton>
#include <QScrollBar>
#include <QResizeEvent>
#include <QDialog>
#include <QKeyEvent>
#include <QScreen>
#include <QApplication>
#include <QFileInfo>
#include <algorithm>

static constexpr int THUMB_SIZE = 200;
static constexpr int COLS       = 3;

// =============================================================================
//  Small clickable thumbnail label
// =============================================================================
class ThumbLabel : public QLabel {
    Q_OBJECT
public:
    ThumbLabel(const QString& path, QWidget* p = nullptr)
        : QLabel(p), path_(path) {
        setCursor(Qt::PointingHandCursor);
        setAlignment(Qt::AlignCenter);
        setFixedSize(THUMB_SIZE, THUMB_SIZE);
        setStyleSheet(
            "QLabel { background:#313244; border:2px solid #45475a;"
            "         border-radius:6px; }"
            "QLabel:hover { border-color:#cba6f7; }");

        QPixmap px(path);
        if (!px.isNull())
            setPixmap(px.scaled(THUMB_SIZE - 8, THUMB_SIZE - 8,
                                Qt::KeepAspectRatio, Qt::SmoothTransformation));
        else
            setText("⚠ No preview");
    }
    const QString& path() const { return path_; }

signals:
    void clicked(const QString& path);

protected:
    void mousePressEvent(QMouseEvent*) override { emit clicked(path_); }
private:
    QString path_;
};

// Required for MOC in .cpp
#include "ImageGallery.moc"

// =============================================================================
//  FullPreview dialog
// =============================================================================
class FullPreview : public QDialog {
public:
    explicit FullPreview(const QString& path, QWidget* parent = nullptr)
        : QDialog(parent, Qt::Dialog | Qt::WindowCloseButtonHint)
    {
        setWindowTitle(QFileInfo(path).fileName());
        setStyleSheet("background:#11111b;");

        QPixmap px(path);
        if (px.isNull()) px.fill(Qt::gray);

        // Fit to 85% of the primary screen
        const QSize screen = QApplication::primaryScreen()->availableSize();
        const QSize maxSz  = screen * 0.85;
        const QPixmap scaled = px.scaled(maxSz, Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation);

        auto* imgLabel = new QLabel(this);
        imgLabel->setPixmap(scaled);
        imgLabel->setAlignment(Qt::AlignCenter);

        // File name + dimensions bar
        auto* info = new QLabel(
            QString("%1   (%2 × %3 px)")
            .arg(QFileInfo(path).fileName())
            .arg(px.width()).arg(px.height()), this);
        info->setStyleSheet("color:#a6adc8; font-size:11px; padding:6px 10px;");
        info->setAlignment(Qt::AlignCenter);

        auto* closeBtn = new QPushButton("✕  Close", this);
        closeBtn->setStyleSheet(
            "QPushButton { background:#313244; color:#cdd6f4; border:none;"
            "              padding:6px 20px; border-radius:4px; }"
            "QPushButton:hover { background:#45475a; }");
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->addWidget(imgLabel, 1);
        layout->addWidget(info);
        layout->addWidget(closeBtn, 0, Qt::AlignCenter);

        resize(scaled.size() + QSize(20, 90));
    }

protected:
    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Return)
            accept();
        else
            QDialog::keyPressEvent(e);
    }
};

// =============================================================================
//  ImageGallery
// =============================================================================
ImageGallery::ImageGallery(QWidget* parent) : QWidget(parent)
{
    buildUI();
}

void ImageGallery::buildUI()
{
    header_ = new QLabel("🖼  Image Gallery");
    header_->setStyleSheet(
        "font-size:14px; font-weight:bold; color:#cba6f7;"
        "padding:10px 12px; background:#181825;");

    // Filter bar
    filterEdit_ = new QLineEdit();
    filterEdit_->setPlaceholderText("Filter images…");
    filterEdit_->setClearButtonEnabled(true);
    filterEdit_->setStyleSheet(
        "QLineEdit { background:#313244; color:#cdd6f4; border:1px solid #585b70;"
        "            padding:5px 8px; border-radius:4px; }"
        "QLineEdit:focus { border-color:#cba6f7; }");
    connect(filterEdit_, &QLineEdit::textChanged,
            this, &ImageGallery::onFilterChanged);

    auto* filterRow = new QWidget();
    filterRow->setStyleSheet("background:#181825;");
    auto* frl = new QHBoxLayout(filterRow);
    frl->setContentsMargins(12, 4, 12, 6);
    frl->addWidget(filterEdit_);

    // Grid inside scroll
    grid_       = new QWidget();
    gridLayout_ = new QGridLayout(grid_);
    gridLayout_->setContentsMargins(12, 12, 12, 12);
    gridLayout_->setSpacing(10);
    grid_->setStyleSheet("background:#1e2230;");

    emptyLabel_ = new QLabel("No images found in this directory.");
    emptyLabel_->setStyleSheet("color:#6c7086; font-size:13px; padding:30px;");
    emptyLabel_->setAlignment(Qt::AlignCenter);

    scroll_ = new QScrollArea();
    scroll_->setWidget(grid_);
    scroll_->setWidgetResizable(true);
    scroll_->setStyleSheet(
        "QScrollArea { border:none; background:#1e2230; }"
        "QScrollBar:vertical { background:#181825; width:8px; border-radius:4px; }"
        "QScrollBar::handle:vertical { background:#585b70; border-radius:4px; }"
        "QScrollBar::handle:vertical:hover { background:#cba6f7; }");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header_);
    layout->addWidget(filterRow);
    layout->addWidget(scroll_, 1);
}

void ImageGallery::setDirectory(const QString& dirPath)
{
    currentDir_ = dirPath;
    filterEdit_->clear();

    QDir dir(dirPath);
    dir.setNameFilters({"*.png", "*.PNG"});
    dir.setSorting(QDir::Name);
    allFiles_.clear();
    for (const auto& fi : dir.entryInfoList(QDir::Files))
        allFiles_ << fi.absoluteFilePath();

    header_->setText(QString("🖼  %1  (%2 images)")
        .arg(QFileInfo(dirPath).fileName()).arg(allFiles_.size()));

    rebuildGrid(allFiles_);
}

void ImageGallery::rebuildGrid(const QStringList& files)
{
    // Clear the grid
    QLayoutItem* item;
    while ((item = gridLayout_->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    if (files.isEmpty()) {
        gridLayout_->addWidget(emptyLabel_, 0, 0, 1, COLS, Qt::AlignCenter);
        emptyLabel_->show();
        return;
    }
    emptyLabel_->hide();

    int col = 0, row = 0;
    for (const QString& path : files) {
        auto* thumb = new ThumbLabel(path, grid_);
        // Label below thumb
        auto* nameLabel = new QLabel(
            QFileInfo(path).completeBaseName(), grid_);
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setStyleSheet("color:#a6adc8; font-size:10px; padding:2px;");
        nameLabel->setMaximumWidth(THUMB_SIZE);

        auto* cell = new QWidget(grid_);
        cell->setFixedWidth(THUMB_SIZE + 4);
        auto* cellL = new QVBoxLayout(cell);
        cellL->setContentsMargins(0, 0, 0, 0);
        cellL->setSpacing(2);
        cellL->addWidget(thumb);
        cellL->addWidget(nameLabel);

        connect(thumb, &ThumbLabel::clicked,
                this, &ImageGallery::onThumbClicked);

        gridLayout_->addWidget(cell, row, col);
        if (++col >= COLS) { col = 0; ++row; }
    }
    // Spacer
    gridLayout_->setRowStretch(row + 1, 1);
}

void ImageGallery::onThumbClicked(const QString& path)
{
    openPreview(path);
}

void ImageGallery::onFilterChanged(const QString& text)
{
    if (text.isEmpty()) {
        rebuildGrid(allFiles_);
        return;
    }
    QStringList filtered;
    for (const auto& f : allFiles_)
        if (QFileInfo(f).fileName().contains(text, Qt::CaseInsensitive))
            filtered << f;
    rebuildGrid(filtered);
}

void ImageGallery::openPreview(const QString& path)
{
    FullPreview dlg(path, this);
    dlg.exec();
}
