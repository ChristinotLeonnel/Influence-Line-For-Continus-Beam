#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QComboBox>
#include <QtCharts/QChartView>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include "JsonUtils.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
QT_CHARTS_USE_NAMESPACE
#endif

// =============================================================================
//  LoadEnvelopePanel
//  Affiche les resultats des enveloppes de chargement :
//    - Tableau des valeurs (maximum, span, section, position)
//    - Graphe en barres comparant les 4 courbes pour un type de charge
// =============================================================================
class LoadEnvelopePanel : public QWidget
{
    Q_OBJECT
public:
    explicit LoadEnvelopePanel(QWidget* parent = nullptr);

    void setPaths(const jutils::Paths& p);
    void showEnvelope(const QString& scope, const QString& loadType);

private slots:
    void onCurveFilterChanged();

private:
    void buildUI();
    void buildBarChart();
    void reloadTable();
    void reloadChart();

    // Widgets
    QLabel*       header_      = nullptr;
    QComboBox*    scopeBox_    = nullptr;
    QComboBox*    loadTypeBox_ = nullptr;
    QTableWidget* table_       = nullptr;
    QChartView*   chartView_   = nullptr;
    QChart*       chart_       = nullptr;

    jutils::Paths paths_;
    bool          loading_ = false;
};
