//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "application.h"
#include "analyzerdefs.h"
#include <qwt_scale_engine.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_marker.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_legenditem.h>
#include <qwt_plot_picker.h>
#include <qwt_symbol.h>
#include <cmath>

struct MainWindow::Impl {
    Ui::MainWindow ui;
    QwtPlotCurve *curve_lo_mag_ = nullptr;
    QwtPlotCurve *curve_lo_phase_ = nullptr;
    QwtPlotCurve *curve_hi_mag_ = nullptr;
    QwtPlotCurve *curve_hi_phase_ = nullptr;
    QwtPlotMarker *marker_mag_ = nullptr;
    QwtPlotMarker *marker_phase_ = nullptr;
    QwtPlotLegendItem *legend_mag_ = nullptr;
    QwtPlotLegendItem *legend_phase_ = nullptr;
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), P(new Impl)
{
    P->ui.setupUi(this);

    for (QwtPlot *plt : {P->ui.pltAmplitude, P->ui.pltPhase}) {
        plt->setAxisScale(QwtPlot::xBottom, Analysis::freq_range_min, Analysis::freq_range_max);
        plt->setAxisScaleEngine(QwtPlot::xBottom, new QwtLogScaleEngine);
        plt->setCanvasBackground(Qt::darkBlue);
        QwtPlotGrid *grid = new QwtPlotGrid;
        grid->setPen(Qt::gray, 0.0, Qt::DotLine);
        grid->attach(plt);
    }

    QColor color_lo(Qt::green);
    QColor color_hi(Qt::red);

    QwtPlotCurve *curve_lo_mag = P->curve_lo_mag_ = new QwtPlotCurve(tr("Lo Signal Gain"));
    curve_lo_mag->attach(P->ui.pltAmplitude);
    curve_lo_mag->setPen(color_lo, 0.0, Qt::SolidLine);
    QwtPlotCurve *curve_lo_phase = P->curve_lo_phase_ = new QwtPlotCurve(tr("Lo Signal Phase"));
    curve_lo_phase->setStyle(QwtPlotCurve::NoCurve);
    QwtSymbol *sym_lo_phase = new QwtSymbol(QwtSymbol::Ellipse, QBrush(Qt::transparent), QPen(color_lo), QSize(6, 6));
    curve_lo_phase->setSymbol(sym_lo_phase);
    curve_lo_phase->attach(P->ui.pltPhase);

    ///
    class AmpPicker : public QwtPlotPicker {
    public:
        using QwtPlotPicker::QwtPlotPicker;

        QwtText trackerText(const QPoint &pos) const override
        {
            QPointF xy = invTransform(pos);
            QwtText text(QString("%0 Hz\n%1 dB")
                         .arg(xy.x(), 0, 'f', 2)
                         .arg(xy.y(), 0, 'f', 2));
            QColor bg(Qt::yellow);
            bg.setAlpha(160);
            text.setBackgroundBrush(bg);
            return text;
        }
    };

    class PhasePicker : public QwtPlotPicker {
    public:
        using QwtPlotPicker::QwtPlotPicker;

        QwtText trackerText(const QPoint &pos) const override
        {
            QPointF xy = invTransform(pos);
            QwtText text(QString::fromUtf8(u8"%0 Hz\n%1 π")
                         .arg(xy.x(), 0, 'f', 2)
                         .arg(xy.y() * (1.0 / M_PI), 0, 'f', 2));
            QColor bg(Qt::yellow);
            bg.setAlpha(160);
            text.setBackgroundBrush(bg);
            return text;
        }
    };

    AmpPicker *amp_picker = new AmpPicker(QwtPlot::xBottom, QwtPlot::yLeft, QwtPicker::CrossRubberBand, QwtPicker::AlwaysOn, P->ui.pltAmplitude->canvas());
    PhasePicker *phase_picker = new PhasePicker(QwtPlot::xBottom, QwtPlot::yLeft, QwtPicker::CrossRubberBand, QwtPicker::AlwaysOn, P->ui.pltPhase->canvas());
    Q_UNUSED(amp_picker);
    Q_UNUSED(phase_picker);
    ///

    QwtPlotCurve *curve_hi_mag = P->curve_hi_mag_ = new QwtPlotCurve(tr("Hi Signal Gain"));
    curve_hi_mag->attach(P->ui.pltAmplitude);
    curve_hi_mag->setPen(color_hi, 0.0, Qt::SolidLine);
    QwtPlotCurve *curve_hi_phase = P->curve_hi_phase_ = new QwtPlotCurve(tr("Hi Signal Phase"));
    curve_hi_phase->setStyle(QwtPlotCurve::NoCurve);
    QwtSymbol *sym_hi_phase = new QwtSymbol(QwtSymbol::Triangle, QBrush(Qt::transparent), QPen(color_hi), QSize(6, 6));
    curve_hi_phase->setSymbol(sym_hi_phase);
    curve_hi_phase->attach(P->ui.pltPhase);

    QwtPlotMarker *marker_mag = P->marker_mag_ = new QwtPlotMarker;
    marker_mag->attach(P->ui.pltAmplitude);
    marker_mag->setLineStyle(QwtPlotMarker::VLine);
    marker_mag->setLinePen(Qt::yellow, 0.0, Qt::DashLine);
    QwtPlotMarker *marker_phase = P->marker_phase_ = new QwtPlotMarker;
    marker_phase->attach(P->ui.pltPhase);
    marker_phase->setLineStyle(QwtPlotMarker::VLine);
    marker_phase->setLinePen(Qt::yellow, 0.0, Qt::DashLine);

    QwtPlotLegendItem *legend_mag = P->legend_mag_ = new QwtPlotLegendItem;
    legend_mag->attach(P->ui.pltAmplitude);
    QwtPlotLegendItem *legend_phase = P->legend_phase_ = new QwtPlotLegendItem;
    legend_phase->attach(P->ui.pltPhase);
    for (QwtPlotLegendItem *x : {legend_mag, legend_phase}) {
        x->setTextPen(QPen(Qt::white));
        x->setBorderPen(QPen(Qt::white));
        QColor bg(Qt::gray);
        bg.setAlpha(160);
        x->setBackgroundBrush(bg);
        x->setAlignment(Qt::AlignRight|Qt::AlignTop);
        x->setMaxColumns(1);
        x->setBackgroundMode(QwtPlotLegendItem::LegendBackground);
        x->setBorderRadius(8);
        x->setMargin(4);
        x->setSpacing(2);
        x->setItemMargin(0);
        QFont f = x->font();
        f.setPointSize(10);
        x->setFont(f);
    }

    P->ui.pltAmplitude->setAxisScale(QwtPlot::yLeft, Analysis::db_range_min, Analysis::db_range_max);
    P->ui.pltPhase->setAxisScale(QwtPlot::yLeft, -M_PI, +M_PI);

    P->ui.sl_gain->setValue(20 * std::log10(Analysis::global_gain));

    connect(P->ui.btn_startSweep, &QAbstractButton::clicked, theApplication, &Application::setSweepActive);
    connect(P->ui.btn_save, &QAbstractButton::clicked, theApplication, &Application::saveProfile);

    connect(
        P->ui.sl_gain, &QwtSlider::valueChanged,
        this, [](double v) { Analysis::global_gain = std::pow(10.0, v * 0.05); });

    P->ui.btn_lo->setChecked(true);
    P->ui.btn_hi->setChecked(true);

    connect(
        P->ui.btn_lo, &QCheckBox::clicked,
        this, [this](bool checked) {
            theApplication->setSweepEnabled(checked, P->ui.btn_hi->isChecked());
            P->curve_lo_mag_->setVisible(checked);
            P->curve_lo_phase_->setVisible(checked);
        });
    connect(
        P->ui.btn_hi, &QCheckBox::clicked,
        this, [this](bool checked) {
            theApplication->setSweepEnabled(P->ui.btn_lo->isChecked(), checked);
            P->curve_hi_mag_->setVisible(checked);
            P->curve_hi_phase_->setVisible(checked);
        });

    P->ui.sp_parallel->setRange(1, Analysis::max_bins_at_once);
    connect(
        P->ui.sp_parallel, QOverload<int>::of(&QSpinBox::valueChanged),
        this, [](int num) { theApplication->setFreqsAtOnce(num); });

    connect(
        theApplication, &Application::sweepPhaseChanged,
        this, [this](int spl) {
                  const char *text;
                  switch (spl) {
                  case Analysis::Signal_Lo: text = "Lo"; break;
                  case Analysis::Signal_Hi: text = "Hi"; break;
                  default: text = "None"; break;
                  }
                  P->ui.lbl_sweep->setText(text);
              });
}

MainWindow::~MainWindow()
{
}

void MainWindow::showCurrentFrequency(float f)
{
    QString text;
    if (f < 1000)
        text = QString::number(std::lround(f)) + " Hz";
    else
        text = QString::number(std::lround(f * 1e-3)) + " kHz";
    P->ui.lbl_frequency->setText(text);
}

void MainWindow::showLevels(float in, float out)
{
    float g_min = P->ui.vu_input->minimum();
    float a_min = std::pow(10.0f, g_min * 0.05f);
    float g_in = (in > a_min) ? (20 * std::log10(in)) : g_min;
    float g_out = (out > a_min) ? (20 * std::log10(out)) : g_min;
    P->ui.vu_input->setValue(g_in);
    P->ui.vu_output->setValue(g_out);
}

void MainWindow::showProgress(float progress)
{
    P->ui.progressBar->setValue(std::lround(progress * 100));
}

void MainWindow::showPlotData(
    const double *freqs, double freqmark,
    const double *lo_mags, const double *lo_phases,
    const double *hi_mags, const double *hi_phases, unsigned n)
{
    P->curve_lo_mag_->setRawSamples(freqs, lo_mags, n);
    P->curve_lo_phase_->setRawSamples(freqs, lo_phases, n);
    P->curve_hi_mag_->setRawSamples(freqs, hi_mags, n);
    P->curve_hi_phase_->setRawSamples(freqs, hi_phases, n);

    P->marker_mag_->setXValue(freqmark);
    P->marker_phase_->setXValue(freqmark);

    P->ui.pltAmplitude->replot();
    P->ui.pltPhase->replot();
}
