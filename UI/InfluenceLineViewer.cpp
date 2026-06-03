#include "InfluenceLineViewer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFormLayout>
#include <QFrame>
#include <QFont>
#include <QGroupBox>
#include <algorithm>
#include <limits>
#include <sstream>
#include <iomanip>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
QT_CHARTS_USE_NAMESPACE
#endif

// =============================================================================
//  Constructor
// =============================================================================
InfluenceLineViewer::InfluenceLineViewer(QWidget* parent)
    : QWidget(parent)
{
    buildChart();
    buildControls();
}

// =============================================================================
//  buildChart
// =============================================================================
void InfluenceLineViewer::buildChart()
{
    chart_ = new QChart();
    chart_->setBackgroundBrush(QColor("#1e2230"));
    chart_->setTitleBrush(QColor("#cdd6f4"));
    chart_->legend()->setLabelColor(QColor("#cdd6f4"));
    chart_->legend()->setBackgroundVisible(true);
    chart_->legend()->setBrush(QColor("#313244"));
    chart_->setMargins(QMargins(8, 8, 8, 8));

    // Series: main influence line
    mainSeries_ = new QLineSeries();
    mainSeries_->setName("Influence Line");
    mainSeries_->setPen(QPen(QColor("#89b4fa"), 2.0));

    // Series: zero line
    zeroSeries_ = new QLineSeries();
    zeroSeries_->setName("Zero");
    QPen zp(QColor("#6c7086"));
    zp.setStyle(Qt::DashLine);
    zeroSeries_->setPen(zp);

    // Series: nodes
    nodeSeries_ = new QScatterSeries();
    nodeSeries_->setName("Nodes");
    nodeSeries_->setColor(QColor("#a6e3a1"));
    nodeSeries_->setMarkerShape(QScatterSeries::MarkerShapeCircle);
    nodeSeries_->setMarkerSize(10);
    nodeSeries_->setBorderColor(QColor("#a6e3a1"));

    // Series: critical value
    critSeries_ = new QScatterSeries();
    critSeries_->setName("Critical");
    critSeries_->setColor(QColor("#f38ba8"));
    critSeries_->setMarkerShape(QScatterSeries::MarkerShapeCircle);
    critSeries_->setMarkerSize(14);
    critSeries_->setBorderColor(Qt::white);

    chart_->addSeries(mainSeries_);
    chart_->addSeries(zeroSeries_);
    chart_->addSeries(nodeSeries_);
    chart_->addSeries(critSeries_);

    chart_->createDefaultAxes();

    // Style axes
    auto axes = chart_->axes();
    for (auto* ax : axes) {
        ax->setLabelsColor(QColor("#cdd6f4"));
        ax->setGridLineColor(QColor("#313244"));
        ax->setLinePenColor(QColor("#585b70"));
    }

    chart_->setTitle("Influence Line");

    chartView_ = new QChartView(chart_);
    chartView_->setRenderHint(QPainter::Antialiasing);
    chartView_->setBackgroundBrush(QColor("#1e2230"));
    chartView_->setMinimumHeight(350);
}

// =============================================================================
//  buildControls
// =============================================================================
void InfluenceLineViewer::buildControls()
{
    // ── Control row ──────────────────────────────────────────────────────────
    auto* ctrl = new QWidget(this);
    ctrl->setStyleSheet("background: #181825; color: #cdd6f4;");
    auto* cLayout = new QHBoxLayout(ctrl);
    cLayout->setContentsMargins(12, 8, 12, 8);
    cLayout->setSpacing(16);

    // Curve selector
    curveBox_ = new QComboBox();
    curveBox_->setStyleSheet(
        "QComboBox { background:#313244; color:#cdd6f4; border:1px solid #585b70;"
        "            padding:4px 8px; border-radius:4px; min-width:140px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background:#313244; color:#cdd6f4; }");
    for (int i = 0; i < 4; ++i)
        curveBox_->addItem(QString::fromLatin1(jutils::curve_labels()[i]));

    // Span selector
    auto* spanLbl = new QLabel("Span:");
    spanLbl->setStyleSheet("color:#a6adc8;");
    spanBox_ = new QSpinBox();
    spanBox_->setMinimum(1); spanBox_->setMaximum(1);
    spanBox_->setStyleSheet(
        "QSpinBox { background:#313244; color:#cdd6f4; border:1px solid #585b70;"
        "           padding:4px; border-radius:4px; min-width:60px; }");

    // Section selector
    auto* secLbl = new QLabel("Section:");
    secLbl->setStyleSheet("color:#a6adc8;");
    sectionBox_ = new QSpinBox();
    sectionBox_->setMinimum(1); sectionBox_->setMaximum(1);
    sectionBox_->setStyleSheet(spanBox_->styleSheet());

    cLayout->addWidget(new QLabel("Curve:"));
    cLayout->addWidget(curveBox_);
    cLayout->addSpacing(12);
    cLayout->addWidget(spanLbl);
    cLayout->addWidget(spanBox_);
    cLayout->addSpacing(12);
    cLayout->addWidget(secLbl);
    cLayout->addWidget(sectionBox_);
    cLayout->addStretch();

    // ── Info bar ─────────────────────────────────────────────────────────────
    infoLabel_ = new QLabel("No project loaded");
    infoLabel_->setStyleSheet("color:#6c7086; padding: 0 12px;");

    critLabel_ = new QLabel();
    critLabel_->setStyleSheet(
        "color:#f38ba8; font-weight:bold; padding: 2px 12px;"
        "background:#313244; border-radius:4px;");
    critLabel_->setVisible(false);

    auto* infoRow = new QWidget(this);
    infoRow->setStyleSheet("background:#181825;");
    auto* infoLayout = new QHBoxLayout(infoRow);
    infoLayout->setContentsMargins(0, 4, 12, 4);
    infoLayout->addWidget(infoLabel_);
    infoLayout->addStretch();
    infoLayout->addWidget(critLabel_);

    // ── Main layout ───────────────────────────────────────────────────────────
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(ctrl);
    layout->addWidget(chartView_, 1);
    layout->addWidget(infoRow);

    // Connections
    connect(curveBox_,   QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &InfluenceLineViewer::onSelectionChanged);
    connect(spanBox_,    QOverload<int>::of(&QSpinBox::valueChanged),
            this, &InfluenceLineViewer::onSpanChanged);
    connect(sectionBox_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &InfluenceLineViewer::onSelectionChanged);
}

// =============================================================================
//  setPaths
// =============================================================================
void InfluenceLineViewer::setPaths(const jutils::Paths& p)
{
    paths_ = p;
    updateChart();
}

void InfluenceLineViewer::showCurve(const QString& curveFile)
{
    for (int i = 0; i < 4; ++i) {
        if (QString::fromLatin1(jutils::curve_files()[i]) == curveFile) {
            loading_ = true;
            curveBox_->setCurrentIndex(i);
            loading_ = false;
            break;
        }
    }
    updateChart();
}

// =============================================================================
//  Slot: span changed -> repopulate section box
// =============================================================================
void InfluenceLineViewer::onSpanChanged(int /*span*/)
{
    populateSectionBox();
    updateChart();
}

void InfluenceLineViewer::onSelectionChanged()
{
    if (loading_) return;
    updateChart();
}

// =============================================================================
//  populateSectionBox
// =============================================================================
void InfluenceLineViewer::populateSectionBox()
{
    if (!paths_.valid()) return;

    const int  ci    = curveBox_->currentIndex();
    const auto fpath = paths_.il_file(jutils::curve_files()[ci]);
    const int  span  = spanBox_->value() - 1;   // 0-based

    try {
        auto data = jutils::load(fpath).get<jutils::Vec3D>();
        if (span < (int)data.size()) {
            const int sections = (int)data[span].size();
            loading_ = true;
            sectionBox_->setMaximum(std::max(1, sections));
            loading_ = false;
        }
    }
    catch (...) {}
}

// =============================================================================
//  updateChart — the core method
// =============================================================================
void InfluenceLineViewer::updateChart()
{
    mainSeries_->clear();
    zeroSeries_->clear();
    nodeSeries_->clear();
    critSeries_->clear();
    critLabel_->setVisible(false);
    chart_->setTitle("Influence Line");

    if (!paths_.valid()) {
        infoLabel_->setText("No project loaded.");
        return;
    }

    const int ci    = curveBox_->currentIndex();
    const int span  = spanBox_->value()    - 1;   // 0-based
    const int sec   = sectionBox_->value() - 1;   // 0-based

    const bool isShear = (ci == 1);   // index 1 = Shear Force

    // ── Load data ─────────────────────────────────────────────────────────────
    jutils::Vec3D   data;
    jutils::Vec1D   abscissa;
    jutils::Vec1D   nodes;

    try {
        data = jutils::load(paths_.il_file(jutils::curve_files()[ci]))
                   .get<jutils::Vec3D>();

        // Update span / section ranges
        loading_ = true;
        spanBox_->setMaximum(std::max(1, (int)data.size()));
        if (span < (int)data.size())
            sectionBox_->setMaximum(std::max(1, (int)data[span].size()));
        loading_ = false;

        if (span >= (int)data.size() || sec >= (int)data[span].size()) {
            infoLabel_->setText("Invalid span/section.");
            return;
        }

        if (isShear) {
            auto xdata = jutils::load(paths_.il_file("shear_abscissa.json"))
                             .get<jutils::Vec3D>();
            if (span < (int)xdata.size() && sec < (int)xdata[span].size())
                abscissa = xdata[span][sec];
        }
        else {
            abscissa = jutils::load(paths_.il_file("abscissa.json"))
                           .get<jutils::Vec1D>();
        }
        nodes = jutils::load(paths_.il_file("node_lengths.json"))
                    .get<jutils::Vec1D>();
    }
    catch (const std::exception& e) {
        infoLabel_->setText(QString("Load error: %1").arg(e.what()));
        return;
    }

    const auto& ydata = data[span][sec];
    if (abscissa.size() != ydata.size()) {
        infoLabel_->setText(
            QString("Size mismatch: x=%1 y=%2")
            .arg(abscissa.size()).arg(ydata.size()));
        return;
    }

    // ── Populate main series ──────────────────────────────────────────────────
    double xmin = std::numeric_limits<double>::max();
    double xmax = -std::numeric_limits<double>::max();
    double ymin = xmin, ymax = -xmin;

    for (std::size_t i = 0; i < abscissa.size(); ++i) {
        const double x = abscissa[i];
        const double y = ydata[i];
        mainSeries_->append(x, y);
        xmin = std::min(xmin, x);  xmax = std::max(xmax, x);
        ymin = std::min(ymin, y);  ymax = std::max(ymax, y);
    }

    // ── Zero line ─────────────────────────────────────────────────────────────
    zeroSeries_->append(xmin, 0.0);
    zeroSeries_->append(xmax, 0.0);

    // ── Node markers ─────────────────────────────────────────────────────────
    for (double nd : nodes)
        nodeSeries_->append(nd, 0.0);

    // ── Critical value marker ─────────────────────────────────────────────────
    try {
        const std::string cvfile = std::string(jutils::curve_files()[ci]);
        auto cvj = jutils::load(paths_.cv_file(cvfile));
        jutils::CriticalValue cv = jutils::CriticalValue::from_json(
            cvj, jutils::curve_labels()[ci]);

        if (cv.span == span && cv.section == sec) {
            const int alpha = cv.alpha;
            if (alpha >= 0 && alpha < (int)abscissa.size()) {
                critSeries_->append(abscissa[alpha], ydata[alpha]);

                std::ostringstream oss;
                oss << "⚡ Critical: " << std::fixed << std::setprecision(4)
                    << cv.value
                    << "  @ x = " << abscissa[alpha]
                    << "  (span " << (cv.span+1)
                    << ", sec "   << cv.section << ")";
                critLabel_->setText(QString::fromStdString(oss.str()));
                critLabel_->setVisible(true);

                ymin = std::min(ymin, ydata[alpha]);
                ymax = std::max(ymax, ydata[alpha]);
            }
        }
    }
    catch (...) {}

    // ── Axes & title ──────────────────────────────────────────────────────────
    const double ymargin = (ymax - ymin) * 0.12 + 1e-9;
    const double xmargin = (xmax - xmin) * 0.02;

    chart_->axes(Qt::Horizontal).first()->setRange(xmin - xmargin, xmax + xmargin);
    chart_->axes(Qt::Vertical  ).first()->setRange(ymin - ymargin, ymax + ymargin);

    chart_->axes(Qt::Horizontal).first()->setTitleText("Position (m)");
    chart_->axes(Qt::Vertical).first()->setTitleText(
        QString::fromLatin1(jutils::curve_labels()[ci]));
    chart_->axes(Qt::Horizontal).first()->setTitleBrush(QColor("#a6adc8"));
    chart_->axes(Qt::Vertical).first()->setTitleBrush(QColor("#a6adc8"));

    chart_->setTitle(
        QString("%1  —  Span %2, Section %3")
        .arg(QString::fromLatin1(jutils::curve_labels()[ci]))
        .arg(span + 1)
        .arg(sec  + 1));

    // ── Info label ────────────────────────────────────────────────────────────
    std::ostringstream info;
    info << jutils::curve_labels()[ci]
         << "  |  "  << abscissa.size() << " points"
         << "  |  y ∈ [" << std::fixed << std::setprecision(4) << ymin
         << ", " << ymax << "]";
    infoLabel_->setText(QString::fromStdString(info.str()));
}
