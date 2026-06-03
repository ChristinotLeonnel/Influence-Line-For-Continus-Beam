#pragma once
#include <QWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QGroupBox>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>
#include "JsonUtils.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
QT_CHARTS_USE_NAMESPACE
#endif

// =============================================================================
//  InfluenceLineViewer
//  Visualisation interactive des lignes d'influence :
//    - Courbe choisie via ComboBox (BM / SF / Def / Rot)
//    - Travee et section choisies via QSpinBox
//    - Le graphe affiche la ligne d'influence + zero + noeuds + max
// =============================================================================
class InfluenceLineViewer : public QWidget
{
    Q_OBJECT
public:
    explicit InfluenceLineViewer(QWidget* parent = nullptr);

    void setPaths(const jutils::Paths& p);
    void showCurve(const QString& curveFile);

private slots:
    void onSelectionChanged();
    void onSpanChanged(int span);

private:
    void buildControls();
    void buildChart();
    void updateChart();
    void populateSectionBox();

    // Widgets
    QComboBox*    curveBox_    = nullptr;
    QSpinBox*     spanBox_     = nullptr;
    QSpinBox*     sectionBox_  = nullptr;
    QLabel*       infoLabel_   = nullptr;
    QLabel*       critLabel_   = nullptr;
    QChartView*   chartView_   = nullptr;

    // Chart elements
    QChart*        chart_      = nullptr;
    QLineSeries*   mainSeries_ = nullptr;   // influence line
    QLineSeries*   zeroSeries_ = nullptr;   // y = 0
    QScatterSeries*nodeSeries_ = nullptr;   // node dots on x-axis
    QScatterSeries*critSeries_ = nullptr;   // critical value marker

    jutils::Paths paths_;
    bool          loading_ = false;   // guard against recursive updates
};
