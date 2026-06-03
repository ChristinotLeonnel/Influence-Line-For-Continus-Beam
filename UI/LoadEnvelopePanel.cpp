#include "LoadEnvelopePanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QScrollArea>
#include <algorithm>
#include <iomanip>
#include <sstream>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
QT_CHARTS_USE_NAMESPACE
#endif

static const QString CB_STYLE =
    "QComboBox { background:#313244; color:#cdd6f4; border:1px solid #585b70;"
    "            padding:4px 8px; border-radius:4px; min-width:130px; }"
    "QComboBox::drop-down { border:none; }"
    "QComboBox QAbstractItemView { background:#313244; color:#cdd6f4; }";

static const QString TABLE_STYLE =
    "QTableWidget { background:#1e2230; color:#cdd6f4; "
    "               gridline-color:#313244; border:none; }"
    "QTableWidget::item { padding:5px 8px; }"
    "QTableWidget::item:selected { background:#45475a; color:#cba6f7; }"
    "QHeaderView::section { background:#181825; color:#a6adc8; "
    "                       border:1px solid #313244; padding:5px; font-weight:bold; }";

// =============================================================================
LoadEnvelopePanel::LoadEnvelopePanel(QWidget* parent) : QWidget(parent)
{
    buildUI();
    buildBarChart();

    connect(scopeBox_,    QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LoadEnvelopePanel::onCurveFilterChanged);
    connect(loadTypeBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LoadEnvelopePanel::onCurveFilterChanged);
}

// =============================================================================
void LoadEnvelopePanel::buildUI()
{
    header_ = new QLabel("📋  Load Envelopes");
    header_->setStyleSheet(
        "font-size:14px; font-weight:bold; color:#cba6f7;"
        "padding:10px 12px; background:#181825;");

    // Controls row
    auto* ctrl = new QWidget();
    ctrl->setStyleSheet("background:#181825; color:#cdd6f4;");
    auto* ctrlL = new QHBoxLayout(ctrl);
    ctrlL->setContentsMargins(12, 6, 12, 6);
    ctrlL->setSpacing(14);

    scopeBox_ = new QComboBox(); scopeBox_->setStyleSheet(CB_STYLE);
    scopeBox_->addItem("Global",           "Global");
    scopeBox_->addItem("Critical Section", "Critical_Section");

    loadTypeBox_ = new QComboBox(); loadTypeBox_->setStyleSheet(CB_STYLE);
    for (int i = 0; i < 3; ++i)
        loadTypeBox_->addItem(
            QString::fromLatin1(jutils::load_type_labels()[i]),
            QString::fromLatin1(jutils::load_type_dirs()[i]));

    ctrlL->addWidget(new QLabel("Scope:"));
    ctrlL->addWidget(scopeBox_);
    ctrlL->addSpacing(10);
    ctrlL->addWidget(new QLabel("Load type:"));
    ctrlL->addWidget(loadTypeBox_);
    ctrlL->addStretch();

    // Table
    table_ = new QTableWidget();
    table_->setColumnCount(6);
    table_->setHorizontalHeaderLabels(
        {"Curve", "Maximum", "Span", "Section", "Position", "Load Detail"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSortingEnabled(true);
    table_->setAlternatingRowColors(true);
    table_->setStyleSheet(TABLE_STYLE);

    // Chart placeholder
    chartView_ = new QChartView();
    chartView_->setMinimumHeight(220);
    chartView_->setBackgroundBrush(QColor("#1e2230"));
    chartView_->setRenderHint(QPainter::Antialiasing);

    // Splitter: table top, chart bottom
    auto* splitter = new QSplitter(Qt::Vertical);
    splitter->addWidget(table_);
    splitter->addWidget(chartView_);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);
    splitter->setStyleSheet("QSplitter::handle { background:#313244; }");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header_);
    layout->addWidget(ctrl);
    layout->addWidget(splitter, 1);
}

// =============================================================================
void LoadEnvelopePanel::buildBarChart()
{
    chart_ = new QChart();
    chart_->setBackgroundBrush(QColor("#1e2230"));
    chart_->setTitleBrush(QColor("#cdd6f4"));
    chart_->legend()->setLabelColor(QColor("#cdd6f4"));
    chart_->legend()->setBackgroundVisible(true);
    chart_->legend()->setBrush(QColor("#313244"));
    chart_->setMargins(QMargins(8, 8, 8, 8));
    chart_->setTitle("Maximum values by curve");
    chartView_->setChart(chart_);
}

// =============================================================================
void LoadEnvelopePanel::setPaths(const jutils::Paths& p)
{
    paths_ = p;
    onCurveFilterChanged();
}

void LoadEnvelopePanel::showEnvelope(const QString& scope, const QString& loadType)
{
    loading_ = true;
    // Match scope
    for (int i = 0; i < scopeBox_->count(); ++i)
        if (scopeBox_->itemData(i).toString() == scope)
            { scopeBox_->setCurrentIndex(i); break; }
    // Match load type
    for (int i = 0; i < loadTypeBox_->count(); ++i)
        if (loadTypeBox_->itemData(i).toString() == loadType)
            { loadTypeBox_->setCurrentIndex(i); break; }
    loading_ = false;
    reloadTable();
    reloadChart();
}

// =============================================================================
void LoadEnvelopePanel::onCurveFilterChanged()
{
    if (loading_) return;
    reloadTable();
    reloadChart();
}

// =============================================================================
void LoadEnvelopePanel::reloadTable()
{
    table_->setRowCount(0);
    if (!paths_.valid()) return;

    const QString scope    = scopeBox_->currentData().toString();
    const QString loadType = loadTypeBox_->currentData().toString();

    int row = 0;
    for (int ci = 0; ci < 4; ++ci) {
        const std::string fname = jutils::curve_files()[ci];
        const std::string lbl   = jutils::curve_labels()[ci];
        const auto fpath = paths_.env_file(scope.toStdString(),
                                           loadType.toStdString(), fname);
        auto j = jutils::load_safe(fpath);
        if (j.is_null()) continue;

        auto res = jutils::EnvelopeResult::from_json(j, lbl,
            loadType.toStdString(), scope.toStdString());

        table_->insertRow(row);
        table_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(res.curve)));

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << res.maximum;
        auto* maxItem = new QTableWidgetItem(QString::fromStdString(oss.str()));
        maxItem->setForeground(res.maximum >= 0 ? QColor("#a6e3a1") : QColor("#f38ba8"));
        table_->setItem(row, 1, maxItem);

        table_->setItem(row, 2, new QTableWidgetItem(QString::number(res.span + 1)));
        table_->setItem(row, 3, new QTableWidgetItem(QString::number(res.section)));

        std::ostringstream pos;
        pos << std::fixed << std::setprecision(3) << res.position;
        table_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(pos.str())));

        // Aggregate load detail in one cell
        std::ostringstream detail;
        for (auto& [nm, le] : res.loads)
            detail << nm << ": " << std::fixed << std::setprecision(3) << le.value << "  ";
        table_->setItem(row, 5, new QTableWidgetItem(
            QString::fromStdString(detail.str())));

        ++row;
    }
    table_->resizeColumnsToContents();
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

// =============================================================================
void LoadEnvelopePanel::reloadChart()
{
    chart_->removeAllSeries();
    for (auto* ax : chart_->axes()) chart_->removeAxis(ax);
    if (!paths_.valid()) return;

    const QString scope    = scopeBox_->currentData().toString();
    const QString loadType = loadTypeBox_->currentData().toString();

    // One bar-set per sign group, one category per curve
    auto* posSet = new QBarSet("Positive");
    posSet->setColor(QColor("#a6e3a1"));
    auto* negSet = new QBarSet("Negative");
    negSet->setColor(QColor("#f38ba8"));

    QStringList categories;
    bool hasData = false;

    for (int ci = 0; ci < 4; ++ci) {
        const std::string fname = jutils::curve_files()[ci];
        const auto fpath = paths_.env_file(scope.toStdString(),
                                           loadType.toStdString(), fname);
        auto j = jutils::load_safe(fpath);
        if (j.is_null()) { *posSet << 0; *negSet << 0; categories << "—"; continue; }
        auto res = jutils::EnvelopeResult::from_json(j, jutils::curve_labels()[ci],
            loadType.toStdString(), scope.toStdString());
        categories << QString::fromLatin1(jutils::curve_labels()[ci]);
        *posSet << (res.maximum >= 0 ? res.maximum : 0.0);
        *negSet << (res.maximum <  0 ? res.maximum : 0.0);
        hasData = true;
    }

    if (!hasData) return;

    auto* series = new QBarSeries();
    series->append(posSet);
    series->append(negSet);
    series->setLabelsVisible(true);
    series->setLabelsPosition(QAbstractBarSeries::LabelsOutsideEnd);
    chart_->addSeries(series);

    auto* axisX = new QBarCategoryAxis();
    axisX->setCategories(categories);
    axisX->setLabelsColor(QColor("#cdd6f4"));
    axisX->setGridLineColor(QColor("#313244"));
    chart_->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto* axisY = new QValueAxis();
    axisY->setLabelsColor(QColor("#cdd6f4"));
    axisY->setGridLineColor(QColor("#313244"));
    axisY->setTitleText("Maximum Value");
    axisY->setTitleBrush(QColor("#a6adc8"));
    chart_->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chart_->setTitle(
        QString("Max Values — %1 / %2")
        .arg(scopeBox_->currentText())
        .arg(loadTypeBox_->currentText()));
}
