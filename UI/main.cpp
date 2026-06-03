#include "MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QTimer>

// =============================================================================
//  Dark system palette (fallback when OS theme is light)
// =============================================================================
// Visual Studio 2022 Dark theme palette
static QPalette vsDarkPalette()
{
    QPalette p;
    // Window surfaces
    p.setColor(QPalette::Window,          QColor("#252526"));  // panel bg
    p.setColor(QPalette::WindowText,      QColor("#D4D4D4"));  // main text
    p.setColor(QPalette::Base,            QColor("#1E1E1E"));  // editor bg
    p.setColor(QPalette::AlternateBase,   QColor("#2D2D30"));  // alternating rows
    p.setColor(QPalette::Text,            QColor("#D4D4D4"));
    p.setColor(QPalette::BrightText,      QColor("#FFFFFF"));
    // Buttons / controls
    p.setColor(QPalette::Button,          QColor("#3F3F46"));
    p.setColor(QPalette::ButtonText,      QColor("#D4D4D4"));
    p.setColor(QPalette::Mid,             QColor("#3F3F46"));
    p.setColor(QPalette::Dark,            QColor("#1E1E1E"));
    // Selection — VS blue
    p.setColor(QPalette::Highlight,       QColor("#007ACC"));
    p.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
    // Links
    p.setColor(QPalette::Link,            QColor("#4EC9B0"));
    p.setColor(QPalette::LinkVisited,     QColor("#569CD6"));
    // Tooltips
    p.setColor(QPalette::ToolTipBase,     QColor("#2D2D30"));
    p.setColor(QPalette::ToolTipText,     QColor("#D4D4D4"));
    // Disabled state
    p.setColor(QPalette::Disabled, QPalette::Text,       QColor("#656565"));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#656565"));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#656565"));
    return p;
}

// =============================================================================
//  main
// =============================================================================
int main(int argc, char* argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    app.setApplicationName("Matrix One Viewer");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("MatrixOne");

    // Use the Fusion style which supports custom palettes on all platforms
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setPalette(vsDarkPalette());

    MainWindow win;
    win.show();

    // If a path was passed as the first argument, open it automatically
    if (argc > 1) {
        const QString arg = QString::fromLocal8Bit(argv[1]);
        if (QDir(arg).exists())
            win.setProperty("autoLoadPath", arg);
            // Actually invoke loadProject via QTimer so the window is shown first:
        QTimer::singleShot(0, &win, [&win, arg]() {
            QMetaObject::invokeMethod(&win, "loadProject",
                Qt::QueuedConnection,
                Q_ARG(QString, arg));
        });
    }

    return app.exec();
}
