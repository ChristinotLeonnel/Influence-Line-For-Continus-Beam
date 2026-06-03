#pragma once
#include <QWidget>
#include <QScrollArea>
#include <QLabel>
#include <QGridLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QString>

// =============================================================================
//  ImageGallery
//  Scrollable thumbnail grid showing all PNG images in a directory.
//  Click a thumbnail to open a full-screen preview overlay.
// =============================================================================
class ImageGallery : public QWidget
{
    Q_OBJECT
public:
    explicit ImageGallery(QWidget* parent = nullptr);
    void setDirectory(const QString& dirPath);

private slots:
    void onThumbClicked(const QString& path);
    void onFilterChanged(const QString& text);

private:
    void buildUI();
    void rebuildGrid(const QStringList& files);
    void openPreview(const QString& path);

    QLabel*       header_     = nullptr;
    QLineEdit*    filterEdit_ = nullptr;
    QScrollArea*  scroll_     = nullptr;
    QWidget*      grid_       = nullptr;
    QGridLayout*  gridLayout_ = nullptr;
    QLabel*       emptyLabel_ = nullptr;

    QString      currentDir_;
    QStringList  allFiles_;
};
