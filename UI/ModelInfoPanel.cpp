#include "ModelInfoPanel.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QScrollArea>
#include <QGroupBox>
#include <sstream>
#include <iomanip>

static const QString TABLE_STYLE =
    "QTableWidget { background:#1e2230; color:#cdd6f4; "
    "               gridline-color:#313244; border:none; }"
    "QTableWidget::item { padding:6px 12px; }"
    "QHeaderView::section { background:#181825; color:#a6adc8; "
    "                       border:1px solid #313244; padding:5px; font-weight:bold; }";

ModelInfoPanel::ModelInfoPanel(QWidget* parent) : QWidget(parent)
{
    header_ = new QLabel("🔧  Structural Model");
    header_->setStyleSheet(
        "font-size:14px; font-weight:bold; color:#cba6f7;"
        "padding:10px 12px; background:#181825;");

    // Key-value summary table (2 cols)
    kvTable_ = new QTableWidget(0, 2);
    kvTable_->setHorizontalHeaderLabels({"Parameter", "Value"});
    kvTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    kvTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    kvTable_->setSelectionMode(QAbstractItemView::NoSelection);
    kvTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    kvTable_->verticalHeader()->setVisible(false);
    kvTable_->setStyleSheet(TABLE_STYLE);
    kvTable_->setMaximumHeight(180);

    // Per-span table
    spanTable_ = new QTableWidget(0, 4);
    spanTable_->setHorizontalHeaderLabels({"Span", "Length (m)", "E (Pa)", "I (m⁴)"});
    spanTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    spanTable_->setSelectionMode(QAbstractItemView::NoSelection);
    spanTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    spanTable_->verticalHeader()->setVisible(false);
    spanTable_->setAlternatingRowColors(true);
    spanTable_->setStyleSheet(TABLE_STYLE);

    auto* spanLabel = new QLabel("📐  Per-Span Properties");
    spanLabel->setStyleSheet(
        "font-size:12px; font-weight:bold; color:#89b4fa;"
        "padding:8px 12px; background:#181825;");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header_);
    layout->addWidget(kvTable_);
    layout->addWidget(spanLabel);
    layout->addWidget(spanTable_, 1);
}

void ModelInfoPanel::setPaths(const jutils::Paths& p)
{
    paths_ = p;
    reload();
}

void ModelInfoPanel::addKV(const QString& key, const QString& value, int row, QColor valColor)
{
    kvTable_->setItem(row, 0, new QTableWidgetItem(key));
    kvTable_->item(row, 0)->setForeground(QColor("#a6adc8"));
    auto* vi = new QTableWidgetItem(value);
    vi->setForeground(valColor);
    kvTable_->setItem(row, 1, vi);
}

void ModelInfoPanel::reload()
{
    kvTable_->setRowCount(0);
    spanTable_->setRowCount(0);
    if (!paths_.valid()) return;

    auto j = jutils::load_safe(paths_.structural_model());
    if (j.is_null()) {
        kvTable_->setRowCount(1);
        addKV("Status", "structural_model.json not found", 0, QColor("#f38ba8"));
        return;
    }

    auto m = jutils::StructuralModel::from_json(j);

    // Summary rows
    const int nKV = 5;
    kvTable_->setRowCount(nKV);
    int r = 0;
    addKV("Number of spans",    QString::number(m.n_spans),      r++, QColor("#89b4fa"));
    addKV("Total nodes",        QString::number(m.n_total_nodes), r++, QColor("#89b4fa"));

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << m.step;
    addKV("Integration step (m)", QString::fromStdString(oss.str()), r++, QColor("#a6e3a1"));

    double totalLen = 0.0;
    for (double v : m.spans) totalLen += v;
    oss.str(""); oss << std::fixed << std::setprecision(3) << totalLen;
    addKV("Total length (m)",   QString::fromStdString(oss.str()), r++, QColor("#a6e3a1"));

    const int nSpans = (int)m.spans.size();
    addKV("Span layout",
        QString("[%1 spans]").arg(nSpans), r++, QColor("#cba6f7"));

    // Per-span rows
    for (int i = 0; i < nSpans; ++i) {
        spanTable_->insertRow(i);
        spanTable_->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));

        auto fmt = [](double v, int prec) {
            std::ostringstream s; s << std::fixed << std::setprecision(prec) << v;
            return QString::fromStdString(s.str());
        };

        spanTable_->setItem(i, 1, new QTableWidgetItem(fmt(
            i < (int)m.spans.size()        ? m.spans[i]        : 0.0, 3)));
        spanTable_->setItem(i, 2, new QTableWidgetItem(fmt(
            i < (int)m.young_modulus.size() ? m.young_modulus[i]: 0.0, 3)));
        spanTable_->setItem(i, 3, new QTableWidgetItem(fmt(
            i < (int)m.inertia.size()       ? m.inertia[i]      : 0.0, 6)));

        for (int c = 0; c < 4; ++c) {
            if (spanTable_->item(i, c))
                spanTable_->item(i, c)->setForeground(QColor("#cdd6f4"));
        }
    }
}
