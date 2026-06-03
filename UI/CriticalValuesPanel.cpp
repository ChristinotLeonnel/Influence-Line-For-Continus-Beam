#include "CriticalValuesPanel.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QFont>
#include <sstream>
#include <iomanip>

static const QString TABLE_STYLE =
    "QTableWidget { background:#1e2230; color:#cdd6f4; "
    "               gridline-color:#313244; border:none; }"
    "QTableWidget::item { padding:6px 10px; }"
    "QTableWidget::item:selected { background:#45475a; color:#cba6f7; }"
    "QHeaderView::section { background:#181825; color:#a6adc8; "
    "                       border:1px solid #313244; padding:6px; font-weight:bold; }";

CriticalValuesPanel::CriticalValuesPanel(QWidget* parent) : QWidget(parent)
{
    header_ = new QLabel("⚡  Critical Values");
    header_->setStyleSheet(
        "font-size:14px; font-weight:bold; color:#cba6f7;"
        "padding:10px 12px; background:#181825;");

    table_ = new QTableWidget();
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({"Curve", "Span", "Section", "Alpha", "Value"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSortingEnabled(true);
    table_->setAlternatingRowColors(true);
    table_->setStyleSheet(TABLE_STYLE);
    table_->setShowGrid(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header_);
    layout->addWidget(table_, 1);
}

void CriticalValuesPanel::setPaths(const jutils::Paths& p)
{
    paths_ = p;
    reload();
}

void CriticalValuesPanel::reload()
{
    table_->setRowCount(0);
    if (!paths_.valid()) return;

    int row = 0;
    for (int i = 0; i < 4; ++i) {
        const std::string fname = jutils::curve_files()[i];
        const std::string lbl   = jutils::curve_labels()[i];
        auto j = jutils::load_safe(paths_.cv_file(fname));
        if (j.is_null()) continue;

        auto cv = jutils::CriticalValue::from_json(j, lbl);

        table_->insertRow(row);
        table_->setItem(row, 0, new QTableWidgetItem(
            QString::fromStdString(cv.curve)));
        table_->setItem(row, 1, new QTableWidgetItem(
            QString::number(cv.span + 1)));
        table_->setItem(row, 2, new QTableWidgetItem(
            QString::number(cv.section)));
        table_->setItem(row, 3, new QTableWidgetItem(
            QString::number(cv.alpha)));

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << cv.value;
        auto* valItem = new QTableWidgetItem(QString::fromStdString(oss.str()));
        valItem->setForeground(
            cv.value >= 0 ? QColor("#a6e3a1") : QColor("#f38ba8"));
        table_->setItem(row, 4, valItem);

        // Highlight the maximum row (abs value)
        for (int c = 0; c < 5; ++c)
            table_->item(row, c)->setBackground(QColor("#2a2d3e"));
        ++row;
    }
    table_->resizeColumnsToContents();
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
}
