#pragma once
#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>
#include <QCheckBox>
#include <QSlider>
#include "BeamViewer.h"
#include "JsonUtils.h"

// =============================================================================
//  BeamPanel
//  Conteneur complet :
//   ┌─────────────────────────────────────────────────────┐
//   │  Header + controles (courbe, type de charge)        │
//   ├─────────────────────────────────────────────────────┤
//   │            BeamViewer (dessin QPainter)             │
//   ├─────────────────────────────────────────────────────┤
//   │  Info bar (section active, valeur critique, alpha)  │
//   └─────────────────────────────────────────────────────┘
// =============================================================================
class BeamPanel : public QWidget
{
    Q_OBJECT
public:
    explicit BeamPanel(QWidget* parent = nullptr);

    void setPaths(const jutils::Paths& p);

    // Synchronisation depuis InfluenceLineViewer
    void syncFromViewer(int curveIndex, int span, int section, double alphaX);

private slots:
    void onCurveChanged(int idx);
    void onLoadTypeChanged(int idx);
    void onSectionClicked(int span, int section);

private:
    void buildUI();
    void updateInfoBar();

    QLabel*       header_       = nullptr;
    QComboBox*    curveBox_     = nullptr;
    QComboBox*    loadTypeBox_  = nullptr;
    BeamViewer*   viewer_       = nullptr;
    QLabel*       infoBar_      = nullptr;

    jutils::Paths paths_;
    int  activeCurve_   = 0;
    int  activeSpan_    = 0;
    int  activeSection_ = 0;
    double alphaX_      = -1.0;
};
