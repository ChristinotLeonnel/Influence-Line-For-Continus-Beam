#pragma once
#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QPainter>
#include <QTimer>
#include <QToolTip>
#include "JsonUtils.h"

// =============================================================================
//  BeamViewer
//  Visualisation graphique de la poutre continue avec :
//    - Appuis (encastrement, rotule, appui simple)
//    - Travees avec cotes
//    - Charges ponctuelles (fleches avec intensite)
//    - Charges reparties (bloc hacture avec intensite)
//    - Position critique alpha (marqueur anime)
//    - Section active (ligne verticale coloree)
//    - Legende interactive
// =============================================================================
class BeamViewer : public QWidget
{
    Q_OBJECT
public:
    explicit BeamViewer(QWidget* parent = nullptr);

    void setPaths(const jutils::Paths& p);

    // Mise a jour depuis l'exterieur (InfluenceLineViewer sync)
    void setActiveCurve(int curveIndex);          // 0=BM 1=SF 2=Def 3=Rot
    void setActiveSpanSection(int span, int sec); // 0-based
    void setAlphaPosition(double xMeters);        // position physique du marqueur

signals:
    void sectionClicked(int span, int section);  // clic sur une section

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
    QSize sizeHint() const override { return {800, 260}; }
    QSize minimumSizeHint() const override { return {500, 200}; }

private:
    // ── Donnees ───────────────────────────────────────────────────────────────
    void loadModel();
    void loadLoads();     // charges depuis structural model input.txt (parsed via JSON)
    void loadEnvelope();  // positions optimales depuis 04_Load_Envelopes

    // ── Dessin ────────────────────────────────────────────────────────────────
    void drawBackground(QPainter&);
    void drawBeam(QPainter&);
    void drawSupports(QPainter&);
    void drawSpanLabels(QPainter&);
    void drawNodes(QPainter&);
    void drawPointLoads(QPainter&);
    void drawDistribLoads(QPainter&);
    void drawAlphaMarker(QPainter&);
    void drawActiveSection(QPainter&);
    void drawLegend(QPainter&);
    void drawTooltipInfo(QPainter&);

    // ── Coord helpers ─────────────────────────────────────────────────────────
    double totalLength() const;
    double meterToPixel(double m) const;  // metres -> pixel X dans la zone de dessin
    QRectF beamRect() const;              // rectangle de la zone poutre

    // ── Donnees du modele ─────────────────────────────────────────────────────
    jutils::Paths            paths_;
    jutils::StructuralModel  model_;
    bool                     modelLoaded_ = false;

    // Charges depuis structural model input.txt
    struct PointLoad  { QString name; double position; double intensity; QColor color; };
    struct DistribLoad{ QString name; double start; double end; double intensity; QColor color; };
    std::vector<PointLoad>  pointLoads_;
    std::vector<DistribLoad> distribLoads_;

    // Position optimale (envelope)
    struct EnvLoad { QString name; double position; double value; };
    std::vector<EnvLoad> envLoads_;

    // ── Etat interactif ───────────────────────────────────────────────────────
    int    activeCurve_  = 0;   // 0=BM 1=SF 2=Def 3=Rot
    int    activeSpan_   = 0;   // 0-based
    int    activeSection_= 0;   // 0-based
    double alphaX_       = -1.0; // metres, -1 = pas de marqueur

    double hoverX_       = -1.0; // pixel X du curseur
    bool   hovering_     = false;

    // ── Animation du marqueur alpha ────────────────────────────────────────────
    QTimer* animTimer_   = nullptr;
    int     animPhase_   = 0;
    bool    animVisible_ = true;

    // ── Geometrie de rendu ────────────────────────────────────────────────────
    static constexpr int MARGIN_LEFT   = 40;
    static constexpr int MARGIN_RIGHT  = 40;
    static constexpr int BEAM_Y_CENTER = 130;  // Y de la ligne de la poutre
    static constexpr int BEAM_THICK    = 10;   // hauteur du rectangle poutre
    static constexpr int SUPPORT_H     = 28;
    static constexpr int ARROW_MAX_H   = 55;   // hauteur max fleche charge ponct.
    static constexpr int DIST_H        = 30;   // hauteur bloc charge repartie
};
