#include "BeamPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <sstream>
#include <iomanip>

static const QString CB_STYLE =
    "QComboBox { background:#313244; color:#cdd6f4; border:1px solid #585b70;"
    "            padding:4px 8px; border-radius:4px; min-width:120px; }"
    "QComboBox::drop-down { border:none; }"
    "QComboBox QAbstractItemView { background:#313244; color:#cdd6f4; }";

// =============================================================================
BeamPanel::BeamPanel(QWidget* parent) : QWidget(parent)
{
    buildUI();
}

// =============================================================================
void BeamPanel::buildUI()
{
    setStyleSheet("background:#1e2230;");

    header_ = new QLabel("🏗  Beam Visualization");
    header_->setStyleSheet(
        "font-size:14px; font-weight:bold; color:#cba6f7;"
        "padding:10px 12px; background:#181825;");

    // Controles
    auto* ctrl = new QWidget();
    ctrl->setStyleSheet("background:#181825; color:#cdd6f4;");
    auto* ctrlL = new QHBoxLayout(ctrl);
    ctrlL->setContentsMargins(12, 6, 12, 6);
    ctrlL->setSpacing(14);

    auto* crvLbl = new QLabel("Courbe :");
    crvLbl->setStyleSheet("color:#a6adc8;");
    curveBox_ = new QComboBox();
    curveBox_->setStyleSheet(CB_STYLE);
    for (int i = 0; i < 4; ++i)
        curveBox_->addItem(QString::fromLatin1(jutils::curve_labels()[i]));

    auto* ltLbl = new QLabel("Charge :");
    ltLbl->setStyleSheet("color:#a6adc8;");
    loadTypeBox_ = new QComboBox();
    loadTypeBox_->setStyleSheet(CB_STYLE);
    for (int i = 0; i < 3; ++i)
        loadTypeBox_->addItem(
            QString::fromLatin1(jutils::load_type_labels()[i]),
            QString::fromLatin1(jutils::load_type_dirs()[i]));

    ctrlL->addWidget(crvLbl);
    ctrlL->addWidget(curveBox_);
    ctrlL->addSpacing(10);
    ctrlL->addWidget(ltLbl);
    ctrlL->addWidget(loadTypeBox_);
    ctrlL->addStretch();

    // Legende rapide
    auto* legLbl = new QLabel(
        "<span style='color:#f38ba8'>▲ Charge ponctuelle</span>"
        "  &nbsp;  "
        "<span style='color:#fab387'>▬ Charge repartie</span>"
        "  &nbsp;  "
        "<span style='color:#a6e3a1'>★ Position optimale</span>"
        "  &nbsp;  "
        "<span style='color:#f38ba8'>◆ Position α</span>");
    legLbl->setStyleSheet("font-size:10px; padding: 0 12px; background:#181825;");
    legLbl->setTextFormat(Qt::RichText);

    // BeamViewer
    viewer_ = new BeamViewer();
    viewer_->setMinimumHeight(220);
    viewer_->setMaximumHeight(280);

    // Barre d'info
    infoBar_ = new QLabel("Cliquer sur la poutre pour selectionner une section");
    infoBar_->setStyleSheet(
        "color:#6c7086; font-size:11px; padding:5px 12px; background:#181825;");

    // Layout principal
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header_);
    layout->addWidget(ctrl);
    layout->addWidget(legLbl);
    layout->addWidget(viewer_, 1);
    layout->addWidget(infoBar_);

    // Connexions
    connect(curveBox_,   QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BeamPanel::onCurveChanged);
    connect(loadTypeBox_,QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BeamPanel::onLoadTypeChanged);
    connect(viewer_,     &BeamViewer::sectionClicked,
            this, &BeamPanel::onSectionClicked);
}

// =============================================================================
void BeamPanel::setPaths(const jutils::Paths& p)
{
    paths_ = p;
    viewer_->setPaths(p);
    updateInfoBar();
}

// =============================================================================
void BeamPanel::syncFromViewer(int curveIndex, int span, int section, double alphaX)
{
    activeCurve_    = curveIndex;
    activeSpan_     = span;
    activeSection_  = section;
    alphaX_         = alphaX;

    // Bloquer les signals pour ne pas reboucler
    QSignalBlocker b(curveBox_);
    curveBox_->setCurrentIndex(curveIndex);

    viewer_->setActiveCurve(curveIndex);
    viewer_->setActiveSpanSection(span, section);
    viewer_->setAlphaPosition(alphaX);
    updateInfoBar();
}

// =============================================================================
void BeamPanel::onCurveChanged(int idx)
{
    activeCurve_ = idx;
    viewer_->setActiveCurve(idx);
    updateInfoBar();
}

void BeamPanel::onLoadTypeChanged(int /*idx*/)
{
    // Recharge les positions optimales avec le nouveau type
    viewer_->setPaths(paths_);
}

void BeamPanel::onSectionClicked(int span, int section)
{
    activeSpan_    = span;
    activeSection_ = section;
    viewer_->setActiveSpanSection(span, section);
    updateInfoBar();
}

// =============================================================================
void BeamPanel::updateInfoBar()
{
    if (!paths_.valid()) {
        infoBar_->setText("Aucun projet chargé.");
        return;
    }

    // Lire la valeur critique pour la courbe active
    const std::string cfile = jutils::curve_files()[activeCurve_];
    auto j = jutils::load_safe(paths_.cv_file(cfile));

    QString info;
    if (!j.is_null()) {
        auto cv = jutils::CriticalValue::from_json(j, jutils::curve_labels()[activeCurve_]);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4) << cv.value;
        info = QString(
            "<span style='color:#a6adc8'>Travee active :</span> "
            "<span style='color:#cba6f7'>T%1</span>  &nbsp; "
            "<span style='color:#a6adc8'>Section :</span> "
            "<span style='color:#89b4fa'>%2</span>  &nbsp; "
            "<span style='color:#a6adc8'>Valeur critique :</span> "
            "<span style='color:%3'>%4</span>  &nbsp; "
            "<span style='color:#a6adc8'>α = </span>"
            "<span style='color:#f38ba8'>%5 m</span>")
            .arg(activeSpan_ + 1)
            .arg(activeSection_)
            .arg(cv.value >= 0 ? "#a6e3a1" : "#f38ba8")
            .arg(QString::fromStdString(oss.str()))
            .arg(alphaX_ >= 0
                 ? QString::number(alphaX_, 'f', 2)
                 : QString("—"));
    } else {
        info = QString(
            "<span style='color:#a6adc8'>Travee :</span> "
            "<span style='color:#cba6f7'>T%1</span>  &nbsp; "
            "<span style='color:#a6adc8'>Section :</span> "
            "<span style='color:#89b4fa'>%2</span>")
            .arg(activeSpan_ + 1)
            .arg(activeSection_);
    }

    infoBar_->setTextFormat(Qt::RichText);
    infoBar_->setText(info);
}
