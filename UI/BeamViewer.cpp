#include "BeamViewer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainterPath>
#include <QMouseEvent>
#include <QFontMetrics>
#include <QToolTip>
#include <cmath>
#include <sstream>
#include <iomanip>

// =============================================================================
//  Palette de couleurs (Catppuccin Mocha)
// =============================================================================
namespace C {
    const QColor beam      ("#89b4fa");   // bleu
    const QColor support   ("#a6e3a1");   // vert
    const QColor node      ("#cba6f7");   // violet
    const QColor alpha     ("#f38ba8");   // rouge
    const QColor section   ("#fab387");   // orange
    const QColor ptLoad    ("#f38ba8");   // rouge
    const QColor distLoad  ("#fab387");   // orange
    const QColor envLoad   ("#a6e3a1");   // vert
    const QColor bg        ("#1e2230");
    const QColor surface   ("#313244");
    const QColor text      ("#cdd6f4");
    const QColor subtext   ("#a6adc8");
    const QColor grid      ("#45475a");

    // couleurs cycliques pour les charges multiples
    inline QColor loadColor(int i) {
        static const QColor pool[] = {
            QColor("#f38ba8"), QColor("#fab387"), QColor("#a6e3a1"),
            QColor("#89b4fa"), QColor("#cba6f7"), QColor("#f9e2af")
        };
        return pool[i % 6];
    }
}

// =============================================================================
//  Constructor
// =============================================================================
BeamViewer::BeamViewer(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(200);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setStyleSheet("background: #1e2230;");

    // Animation alpha (clignotement)
    animTimer_ = new QTimer(this);
    connect(animTimer_, &QTimer::timeout, this, [this]() {
        ++animPhase_;
        animVisible_ = (animPhase_ % 8) < 5;   // 5/8 visible
        if (alphaX_ >= 0) update();
    });
    animTimer_->start(120);
}

// =============================================================================
//  setPaths
// =============================================================================
void BeamViewer::setPaths(const jutils::Paths& p)
{
    paths_       = p;
    modelLoaded_ = false;
    pointLoads_.clear();
    distribLoads_.clear();
    envLoads_.clear();
    loadModel();
    loadLoads();
    loadEnvelope();
    update();
}

// =============================================================================
//  loadModel
// =============================================================================
void BeamViewer::loadModel()
{
    auto j = jutils::load_safe(paths_.structural_model());
    if (j.is_null()) return;
    model_      = jutils::StructuralModel::from_json(j);
    modelLoaded_= true;
}

// =============================================================================
//  loadLoads — lit les charges depuis 01_Input/structural_model.json
//  (les charges sont aussi stockees dans le JSON du modele si Output.cpp
//   les y inclut, sinon on relit structural model input.txt via la cle "loads")
// =============================================================================
void BeamViewer::loadLoads()
{
    pointLoads_.clear();
    distribLoads_.clear();
    if (!modelLoaded_) return;

    auto j = jutils::load_safe(paths_.structural_model());
    if (j.is_null()) return;

    // ── Charges ponctuelles ───────────────────────────────────────────────────
    if (j.contains("point_loads") && j["point_loads"].is_array()) {
        int ci = 0;
        for (auto& item : j["point_loads"]) {
            PointLoad pl;
            pl.name      = QString::fromStdString(item.value("name",     "P"));
            pl.position  = item.value("position",  0.0);
            pl.intensity = item.value("intensity",  1.0);
            pl.color     = C::loadColor(ci++);
            pointLoads_.push_back(pl);
        }
    }

    // ── Charges reparties ─────────────────────────────────────────────────────
    if (j.contains("distributed_loads") && j["distributed_loads"].is_array()) {
        int ci = 0;
        for (auto& item : j["distributed_loads"]) {
            DistribLoad dl;
            dl.name      = QString::fromStdString(item.value("name",      "q"));
            dl.start     = item.value("start",     0.0);
            dl.end       = item.value("end",       1.0);
            dl.intensity = item.value("intensity",  1.0);
            dl.color     = C::loadColor(ci++ + 3);
            distribLoads_.push_back(dl);
        }
    }

    // Fallback: si le modele n'a pas de charges stockees, on genere
    // des charges symboliques a partir des travees pour l'affichage
    if (pointLoads_.empty() && distribLoads_.empty()) {
        // Charge ponctuelle au milieu de la 1ere travee (demo)
        if (!model_.spans.empty()) {
            PointLoad pl;
            pl.name     = "P";
            pl.position = model_.spans[0] / 2.0;
            pl.intensity= 1.0;
            pl.color    = C::ptLoad;
            pointLoads_.push_back(pl);
        }
    }
}

// =============================================================================
//  loadEnvelope — positions optimales de chargement
// =============================================================================
void BeamViewer::loadEnvelope()
{
    envLoads_.clear();
    if (!modelLoaded_) return;

    const char* file = jutils::curve_files()[activeCurve_];
    const auto fpath = paths_.env_file("Global", "Point_Load", file);
    auto j = jutils::load_safe(fpath);
    if (j.is_null()) return;

    auto res = jutils::EnvelopeResult::from_json(j,
        jutils::curve_labels()[activeCurve_], "Point_Load", "Global");

    for (auto& [nm, le] : res.loads) {
        EnvLoad el;
        el.name     = QString::fromStdString(nm);
        el.position = le.position;
        el.value    = le.value;
        envLoads_.push_back(el);
    }
}

// =============================================================================
//  Setters externes
// =============================================================================
void BeamViewer::setActiveCurve(int ci) {
    activeCurve_ = ci;
    loadEnvelope();
    update();
}
void BeamViewer::setActiveSpanSection(int span, int sec) {
    activeSpan_ = span; activeSection_ = sec; update();
}
void BeamViewer::setAlphaPosition(double xm) {
    alphaX_ = xm; update();
}

// =============================================================================
//  Geometry helpers
// =============================================================================
double BeamViewer::totalLength() const {
    double s = 0; for (double l : model_.spans) s += l; return s;
}

double BeamViewer::meterToPixel(double m) const {
    const double total = totalLength();
    if (total <= 0) return MARGIN_LEFT;
    const double avail = width() - MARGIN_LEFT - MARGIN_RIGHT;
    return MARGIN_LEFT + m / total * avail;
}

QRectF BeamViewer::beamRect() const {
    return {(double)MARGIN_LEFT,
            (double)(BEAM_Y_CENTER - BEAM_THICK / 2),
            (double)(width() - MARGIN_LEFT - MARGIN_RIGHT),
            (double)BEAM_THICK};
}

// =============================================================================
//  Mouse events
// =============================================================================
void BeamViewer::mouseMoveEvent(QMouseEvent* e) {
    hoverX_  = e->position().x();
    hovering_= true;
    update();
    QWidget::mouseMoveEvent(e);
}
void BeamViewer::leaveEvent(QEvent* e) {
    hovering_ = false; update(); QWidget::leaveEvent(e);
}
void BeamViewer::mousePressEvent(QMouseEvent* e) {
    if (!modelLoaded_ || model_.spans.empty()) return;
    // Determine which section was clicked
    const double xm = (e->position().x() - MARGIN_LEFT) /
                      (width() - MARGIN_LEFT - MARGIN_RIGHT) * totalLength();
    double acc = 0.0;
    for (int sp = 0; sp < (int)model_.spans.size(); ++sp) {
        const double next = acc + model_.spans[sp];
        if (xm >= acc && xm <= next) {
            // Section = node index within span
            int sec = 0;
            if (!model_.node_lengths.empty()) {
                double best = 1e18;
                for (int k = 0; k < (int)model_.node_lengths.size(); ++k) {
                    if (std::abs(model_.node_lengths[k] - xm) < best) {
                        best = std::abs(model_.node_lengths[k] - xm);
                        sec  = k;
                    }
                }
            }
            emit sectionClicked(sp, sec);
            break;
        }
        acc = next;
    }
}

// =============================================================================
//  paintEvent — chef d'orchestre
// =============================================================================
void BeamViewer::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    drawBackground(p);
    if (!modelLoaded_ || model_.spans.empty()) {
        p.setPen(C::subtext);
        p.setFont(QFont("Segoe UI", 11));
        p.drawText(rect(), Qt::AlignCenter, "Aucun projet chargé");
        return;
    }
    drawDistribLoads(p);
    drawBeam(p);
    drawSupports(p);
    drawSpanLabels(p);
    drawNodes(p);
    drawPointLoads(p);
    drawActiveSection(p);
    drawAlphaMarker(p);
    drawLegend(p);
    if (hovering_) drawTooltipInfo(p);
}

// =============================================================================
//  drawBackground
// =============================================================================
void BeamViewer::drawBackground(QPainter& p)
{
    p.fillRect(rect(), C::bg);

    // Ligne de sol (ground line)
    const int groundY = BEAM_Y_CENTER + SUPPORT_H + 18;
    QPen gp(C::grid, 1, Qt::DashLine);
    p.setPen(gp);
    p.drawLine(MARGIN_LEFT - 10, groundY, width() - MARGIN_RIGHT + 10, groundY);

    // Hachures du sol
    p.setPen(QPen(C::grid, 1));
    for (int x = MARGIN_LEFT - 10; x < width() - MARGIN_RIGHT + 10; x += 10) {
        p.drawLine(x, groundY, x - 6, groundY + 8);
    }
}

// =============================================================================
//  drawBeam
// =============================================================================
void BeamViewer::drawBeam(QPainter& p)
{
    const QRectF r = beamRect();
    // Ombre
    p.fillRect(r.adjusted(2, 2, 2, 2), QColor(0, 0, 0, 60));
    // Corps
    QLinearGradient g(0, r.top(), 0, r.bottom());
    g.setColorAt(0.0, C::beam.lighter(130));
    g.setColorAt(0.5, C::beam);
    g.setColorAt(1.0, C::beam.darker(130));
    p.fillRect(r, g);
    // Contour
    p.setPen(QPen(C::beam.lighter(150), 1));
    p.drawRect(r);
}

// =============================================================================
//  drawSupports
// =============================================================================
void BeamViewer::drawSupports(QPainter& p)
{
    if (model_.node_lengths.empty()) return;

    for (int i = 0; i < (int)model_.node_lengths.size(); ++i) {
        const double px = meterToPixel(model_.node_lengths[i]);
        const int    bY = BEAM_Y_CENTER + BEAM_THICK / 2;
        const int    sY = bY + SUPPORT_H;
        const bool   isFirst = (i == 0);
        const bool   isLast  = (i == (int)model_.node_lengths.size() - 1);

        p.setPen(QPen(C::support, 1.5));
        p.setBrush(C::support.darker(160));

        if (isFirst) {
            // Appui encastrement (triangle plein + base)
            QPolygonF tri;
            tri << QPointF(px, bY)
                << QPointF(px - 14, sY)
                << QPointF(px + 14, sY);
            p.drawPolygon(tri);
            p.setPen(QPen(C::support, 2));
            p.drawLine((int)px - 14, sY + 2, (int)px + 14, sY + 2);
        } else if (isLast) {
            // Appui simple (triangle vide)
            QPolygonF tri;
            tri << QPointF(px, bY)
                << QPointF(px - 12, sY)
                << QPointF(px + 12, sY);
            p.setBrush(Qt::NoBrush);
            p.drawPolygon(tri);
            // Cercle en bas
            p.setBrush(C::support);
            p.drawEllipse(QPointF(px, sY + 4), 4, 4);
        } else {
            // Appui intermediaire (triangle + rouleau)
            QPolygonF tri;
            tri << QPointF(px, bY)
                << QPointF(px - 11, sY)
                << QPointF(px + 11, sY);
            p.drawPolygon(tri);
            // 3 petits cercles symbolisant les rouleaux
            p.setPen(QPen(C::support, 1));
            p.setBrush(C::bg);
            for (int k = -1; k <= 1; ++k)
                p.drawEllipse(QPointF(px + k * 7, sY + 5), 3.5, 3.5);
        }

        // Etiquette du noeud
        p.setPen(C::subtext);
        p.setFont(QFont("Segoe UI", 8));
        const QString lbl = QString("N%1\n%2m").arg(i+1)
                            .arg(model_.node_lengths[i], 0, 'f', 1);
        p.drawText(QRectF(px - 20, sY + 14, 40, 28),
                   Qt::AlignHCenter | Qt::TextWordWrap, lbl);
    }
}

// =============================================================================
//  drawSpanLabels
// =============================================================================
void BeamViewer::drawSpanLabels(QPainter& p)
{
    double acc = 0.0;
    for (int sp = 0; sp < (int)model_.spans.size(); ++sp) {
        const double L    = model_.spans[sp];
        const double mid  = meterToPixel(acc + L / 2.0);
        const double x0   = meterToPixel(acc);
        const double x1   = meterToPixel(acc + L);
        const int    cotY = BEAM_Y_CENTER - BEAM_THICK / 2 - 32;

        // Ligne de cote
        p.setPen(QPen(C::subtext, 0.8));
        p.drawLine((int)x0, cotY + 8, (int)x0, cotY + 18);
        p.drawLine((int)x1, cotY + 8, (int)x1, cotY + 18);
        p.drawLine((int)x0 + 4, cotY + 13, (int)x1 - 4, cotY + 13);
        // Fleches de cote
        QPolygonF arL, arR;
        arL << QPointF(x0 + 4, cotY + 13)
            << QPointF(x0 + 10, cotY + 10)
            << QPointF(x0 + 10, cotY + 16);
        arR << QPointF(x1 - 4, cotY + 13)
            << QPointF(x1 - 10, cotY + 10)
            << QPointF(x1 - 10, cotY + 16);
        p.setBrush(C::subtext);
        p.setPen(Qt::NoPen);
        p.drawPolygon(arL);
        p.drawPolygon(arR);

        // Label de la travee
        p.setPen(C::text);
        p.setFont(QFont("Segoe UI", 9, QFont::Medium));
        const bool isActive = (sp == activeSpan_);
        if (isActive) {
            p.setPen(C::section);
            p.setFont(QFont("Segoe UI", 9, QFont::Bold));
        }
        p.drawText(QRectF(mid - 40, cotY - 14, 80, 16),
                   Qt::AlignHCenter | Qt::AlignVCenter,
                   QString("T%1 — %2 m").arg(sp + 1).arg(L, 0, 'f', 2));

        acc += L;
    }
}

// =============================================================================
//  drawNodes — points sur la poutre
// =============================================================================
void BeamViewer::drawNodes(QPainter& p)
{
    for (int i = 0; i < (int)model_.node_lengths.size(); ++i) {
        const double px = meterToPixel(model_.node_lengths[i]);
        const int    beamTop = BEAM_Y_CENTER - BEAM_THICK / 2;
        const int    beamBot = BEAM_Y_CENTER + BEAM_THICK / 2;

        // Trait vertical sur la poutre
        p.setPen(QPen(C::node, 1, Qt::DotLine));
        p.drawLine((int)px, beamTop - 4, (int)px, beamBot + 4);

        // Point central
        p.setPen(Qt::NoPen);
        p.setBrush(C::node);
        p.drawEllipse(QPointF(px, BEAM_Y_CENTER), 4, 4);
    }
}

// =============================================================================
//  drawPointLoads — fleches de charges ponctuelles
// =============================================================================
void BeamViewer::drawPointLoads(QPainter& p)
{
    const int beamTop = BEAM_Y_CENTER - BEAM_THICK / 2;
    const int arrowY  = beamTop - 6;

    // ── Charges du modele (positions fixes) ───────────────────────────────────
    for (int i = 0; i < (int)pointLoads_.size(); ++i) {
        const auto& pl = pointLoads_[i];
        const double px = meterToPixel(pl.position);
        const int    h  = ARROW_MAX_H;
        const QColor& c = pl.color;

        // Corps de la fleche
        p.setPen(QPen(c, 2));
        p.drawLine((int)px, arrowY - h, (int)px, arrowY);

        // Pointe
        QPolygonF head;
        head << QPointF(px,     arrowY)
             << QPointF(px - 5, arrowY - 10)
             << QPointF(px + 5, arrowY - 10);
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawPolygon(head);

        // Label
        p.setPen(c);
        p.setFont(QFont("Segoe UI", 8, QFont::Bold));
        const QString lbl = QString("%1\n%2 kN")
            .arg(pl.name)
            .arg(pl.intensity, 0, 'f', 1);
        p.drawText(QRectF(px + 5, arrowY - h - 2, 60, 36),
                   Qt::TextWordWrap, lbl);
    }

    // ── Positions optimales (envelope) ────────────────────────────────────────
    for (int i = 0; i < (int)envLoads_.size(); ++i) {
        const auto& el = envLoads_[i];
        if (el.position < 0) continue;
        const double px = meterToPixel(el.position);
        const int    h  = 20;

        // Fleche verte plus courte
        p.setPen(QPen(C::envLoad, 1.5, Qt::DashLine));
        p.drawLine((int)px, arrowY - h, (int)px, arrowY);
        QPolygonF head;
        head << QPointF(px,     arrowY)
             << QPointF(px - 4, arrowY - 8)
             << QPointF(px + 4, arrowY - 8);
        p.setBrush(C::envLoad);
        p.setPen(Qt::NoPen);
        p.drawPolygon(head);

        // Label
        p.setPen(C::envLoad);
        p.setFont(QFont("Segoe UI", 7));
        p.drawText(QRectF(px + 4, arrowY - h - 12, 55, 22),
                   Qt::TextWordWrap,
                   QString("%1★").arg(el.name));
    }
}

// =============================================================================
//  drawDistribLoads — blocs hachures pour les charges reparties
// =============================================================================
void BeamViewer::drawDistribLoads(QPainter& p)
{
    const int beamTop = BEAM_Y_CENTER - BEAM_THICK / 2;
    const int blockY  = beamTop - DIST_H - 8;

    for (int i = 0; i < (int)distribLoads_.size(); ++i) {
        const auto& dl = distribLoads_[i];
        const double x0 = meterToPixel(dl.start);
        const double x1 = meterToPixel(dl.end);
        const QColor& c = dl.color;
        const double w  = x1 - x0;
        if (w < 2) continue;

        // Rectangle de fond semi-transparent
        QColor fill = c;
        fill.setAlpha(40);
        p.fillRect(QRectF(x0, blockY, w, DIST_H), fill);

        // Contour
        p.setPen(QPen(c, 1));
        p.drawRect(QRectF(x0, blockY, w, DIST_H));

        // Hachures diagonales
        p.setPen(QPen(c, 0.7, Qt::SolidLine));
        p.setClipRect(QRectF(x0, blockY, w, DIST_H));
        for (double hx = x0 - DIST_H; hx < x1 + DIST_H; hx += 8) {
            p.drawLine((int)hx, blockY, (int)(hx + DIST_H), blockY + DIST_H);
        }
        p.setClipping(false);

        // Petites fleches vers le bas le long du bloc
        p.setPen(QPen(c, 1.5));
        for (double ax = x0 + 8; ax < x1 - 4; ax += 16) {
            p.drawLine((int)ax, blockY, (int)ax, beamTop - 2);
            QPolygonF tip;
            tip << QPointF(ax,     beamTop - 2)
                << QPointF(ax - 3, beamTop - 7)
                << QPointF(ax + 3, beamTop - 7);
            p.setBrush(c);
            p.setPen(Qt::NoPen);
            p.drawPolygon(tip);
            p.setPen(QPen(c, 1.5));
        }

        // Label au centre du bloc
        p.setPen(c);
        p.setFont(QFont("Segoe UI", 8, QFont::Bold));
        p.drawText(QRectF(x0, blockY, w, DIST_H),
                   Qt::AlignCenter,
                   QString("%1: %2 kN/m").arg(dl.name).arg(dl.intensity, 0, 'f', 1));
    }
}

// =============================================================================
//  drawActiveSection — ligne verticale sur la section active
// =============================================================================
void BeamViewer::drawActiveSection(QPainter& p)
{
    if (!modelLoaded_ || model_.node_lengths.empty()) return;
    if (activeSection_ < 0 || activeSection_ >= (int)model_.node_lengths.size())
        return;

    const double px  = meterToPixel(model_.node_lengths[activeSection_]);
    const int    top = BEAM_Y_CENTER - BEAM_THICK / 2 - 60;
    const int    bot = BEAM_Y_CENTER + SUPPORT_H + 12;

    // Ligne translucide
    QColor sc = C::section;
    sc.setAlpha(100);
    p.setPen(QPen(sc, 1.5, Qt::DashDotLine));
    p.drawLine((int)px, top, (int)px, bot);

    // Badge "Section active"
    p.setPen(C::section);
    p.setFont(QFont("Segoe UI", 7, QFont::Bold));
    const QRectF badge(px - 22, top - 16, 44, 14);
    p.fillRect(badge, QColor(250, 179, 135, 40));
    p.setPen(QPen(C::section, 0.8));
    p.drawRect(badge);
    p.setPen(C::section);
    p.drawText(badge, Qt::AlignCenter, "Section");
}

// =============================================================================
//  drawAlphaMarker — marqueur anime de la position alpha
// =============================================================================
void BeamViewer::drawAlphaMarker(QPainter& p)
{
    if (alphaX_ < 0 || !animVisible_) return;

    const double px  = meterToPixel(alphaX_);
    const int    top = BEAM_Y_CENTER - BEAM_THICK / 2 - 8;
    const int    bot = BEAM_Y_CENTER + BEAM_THICK / 2 + 8;

    // Halo
    QRadialGradient halo(px, BEAM_Y_CENTER, 16);
    halo.setColorAt(0.0, QColor(243, 139, 168, 120));
    halo.setColorAt(1.0, QColor(243, 139, 168, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(halo);
    p.drawEllipse(QPointF(px, BEAM_Y_CENTER), 16, 16);

    // Ligne verticale
    p.setPen(QPen(C::alpha, 2));
    p.drawLine((int)px, top, (int)px, bot);

    // Diamant central
    QPolygonF diamond;
    diamond << QPointF(px,     BEAM_Y_CENTER - 7)
            << QPointF(px + 5, BEAM_Y_CENTER)
            << QPointF(px,     BEAM_Y_CENTER + 7)
            << QPointF(px - 5, BEAM_Y_CENTER);
    p.setBrush(C::alpha);
    p.setPen(QPen(Qt::white, 1));
    p.drawPolygon(diamond);

    // Etiquette
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << alphaX_ << " m";
    const QString lbl = QString("α = %1").arg(QString::fromStdString(oss.str()));
    p.setFont(QFont("Segoe UI", 8, QFont::Bold));
    const QFontMetrics fm(p.font());
    const QRectF  lr(px - fm.horizontalAdvance(lbl) / 2.0 - 4,
                     top - 20, fm.horizontalAdvance(lbl) + 8, 15);
    p.fillRect(lr, QColor(243, 139, 168, 50));
    p.setPen(QPen(C::alpha, 0.8));
    p.drawRect(lr);
    p.setPen(C::alpha);
    p.drawText(lr, Qt::AlignCenter, lbl);
}

// =============================================================================
//  drawLegend
// =============================================================================
void BeamViewer::drawLegend(QPainter& p)
{
    struct LegItem { QColor c; QString lbl; };
    QVector<LegItem> items = {
        { C::beam,     "Poutre" },
        { C::support,  "Appuis" },
        { C::node,     "Noeuds" },
        { C::alpha,    "Position α" },
        { C::section,  "Section active" },
    };
    if (!envLoads_.empty())
        items.push_back({ C::envLoad, "Charge optimale ★" });

    const int legX = width() - MARGIN_RIGHT - 140;
    const int legY = 8;
    const int row  = 14;
    p.setFont(QFont("Segoe UI", 8));

    for (int i = 0; i < items.size(); ++i) {
        const int y = legY + i * row;
        p.setPen(Qt::NoPen);
        p.setBrush(items[i].c);
        p.drawEllipse(legX, y + 2, 8, 8);
        p.setPen(C::subtext);
        p.drawText(legX + 12, y + 10, items[i].lbl);
    }
}

// =============================================================================
//  drawTooltipInfo — info sous le curseur
// =============================================================================
void BeamViewer::drawTooltipInfo(QPainter& p)
{
    if (!modelLoaded_ || model_.spans.empty()) return;

    const double avail = width() - MARGIN_LEFT - MARGIN_RIGHT;
    if (avail <= 0) return;
    const double xm = (hoverX_ - MARGIN_LEFT) / avail * totalLength();
    if (xm < 0 || xm > totalLength()) return;

    // Identifier la travee
    double acc = 0.0;
    int    sp  = 0;
    for (int i = 0; i < (int)model_.spans.size(); ++i) {
        if (xm <= acc + model_.spans[i]) { sp = i; break; }
        acc += model_.spans[i];
    }
    const double xInSpan = xm - acc;

    // Barre de position
    p.setPen(QPen(QColor(255,255,255,40), 1, Qt::DotLine));
    p.drawLine((int)hoverX_, BEAM_Y_CENTER - 60, (int)hoverX_, BEAM_Y_CENTER + 40);

    // Tooltip
    const QString info = QString("x = %1 m  |  T%2  |  +%3 m dans T%2")
        .arg(xm, 0, 'f', 2)
        .arg(sp + 1)
        .arg(xInSpan, 0, 'f', 2);
    p.setFont(QFont("Segoe UI", 8));
    const QFontMetrics fm(p.font());
    const int tw = fm.horizontalAdvance(info) + 12;
    int tx = (int)hoverX_ + 8;
    if (tx + tw > width() - 4) tx = (int)hoverX_ - tw - 8;
    const QRect tip(tx, BEAM_Y_CENTER - 75, tw, 16);
    p.fillRect(tip, QColor(49, 50, 68, 220));
    p.setPen(QPen(C::grid, 0.8));
    p.drawRect(tip);
    p.setPen(C::text);
    p.drawText(tip, Qt::AlignCenter, info);
}
