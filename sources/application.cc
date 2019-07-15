//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "application.h"
#include "mainwindow.h"
#include "audioprocessor.h"
#include "analyzerdefs.h"
#include "messages.h"
#include "utility/counting_bitset.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QDebug>
#include <fstream>
#include <iomanip>
#include <complex>
#include <cmath>
#include <cassert>
typedef std::complex<float> cfloat;

struct Application::Impl {
    Audio_Processor *proc_ = nullptr;
    MainWindow *mainwindow_ = nullptr;
    QTimer *tm_rtupdates_ = nullptr;
    QTimer *tm_nextsweep_ = nullptr;

    std::unique_ptr<double[]> an_freqs_;
    std::unique_ptr<cfloat[]> an_lo_response_;
    std::unique_ptr<cfloat[]> an_hi_response_;

    std::unique_ptr<double[]> an_lo_plot_mags_;
    std::unique_ptr<double[]> an_lo_plot_phases_;
    std::unique_ptr<double[]> an_hi_plot_mags_;
    std::unique_ptr<double[]> an_hi_plot_phases_;

    bool sweep_active_ = false;
    unsigned sweep_index_ = 0;
    int sweep_spl_ = Analysis::Signal_Lo;
    unsigned freqs_at_once_ = 1;
    counting_bitset<Analysis::sweep_length> sweep_progress_;

    bool lo_enable_ = true;
    bool hi_enable_ = true;
    int next_spl_phase(int spl) const;
    bool enabled_spl(int spl) const;
    void set_sweep_phase(int spl);
};

Application::Application(int &argc, char *argv[])
    : QApplication(argc, argv), P(new Impl)
{
    setApplicationName("Spectral Profiler");

    QTimer *tm;

    tm = P->tm_rtupdates_ = new QTimer(this);
    connect(tm, &QTimer::timeout, this, &Application::realtimeUpdateTick);
    tm->start(50);

    tm = P->tm_nextsweep_ = new QTimer(this);
    tm->setSingleShot(true);
    connect(tm, &QTimer::timeout, this, &Application::nextSweepTick);
}

Application::~Application()
{
}

void Application::setAudioProcessor(Audio_Processor &proc)
{
    P->proc_ = &proc;

    const unsigned ns = Analysis::sweep_length;
    double *freqs = new double[ns];
    P->an_freqs_.reset(freqs);

    for (unsigned i = 0; i < ns; ++i) {
        const double lx1 = std::log10((double)Analysis::freq_range_min);
        const double lx2 = std::log10((double)Analysis::freq_range_max);
        double r = (double)i / (ns - 1);
        freqs[i] = std::pow(10.0, lx1 + r * (lx2 - lx1));
    }

    P->an_lo_response_.reset(new cfloat[ns]());
    P->an_hi_response_.reset(new cfloat[ns]());

    P->an_lo_plot_mags_.reset(new double[ns]());
    P->an_lo_plot_phases_.reset(new double[ns]());
    P->an_hi_plot_mags_.reset(new double[ns]());
    P->an_hi_plot_phases_.reset(new double[ns]());
}

void Application::setMainWindow(MainWindow &win)
{
    P->mainwindow_ = &win;
}

void Application::setSweepEnabled(bool lo, bool hi)
{
    if (lo == P->lo_enable_ && hi == P->hi_enable_)
        return;

    bool disabled = !P->lo_enable_ && !P->hi_enable_;
    P->lo_enable_ = lo;
    P->hi_enable_ = hi;

    P->sweep_progress_.reset();
    P->mainwindow_->showProgress(0);

    if (disabled) {
        int next = P->next_spl_phase(P->sweep_spl_);
        if (next != -1) {
            P->set_sweep_phase(next);
            P->tm_nextsweep_->start(0);
        }
    }
}

void Application::setFreqsAtOnce(unsigned count)
{
    P->freqs_at_once_ = count;
}

void Application::setSweepActive(bool active)
{
    if (P->sweep_active_ == active)
        return;

    P->sweep_active_ = active;
    if (!active) {
        P->tm_nextsweep_->stop();

        Messages::RequestStop msg;
        P->proc_->send_message(msg);
    }
    else {
        P->sweep_progress_.reset();
        P->mainwindow_->showProgress(0);
        P->tm_nextsweep_->start(0);
    }
}

void Application::saveProfile()
{
    QString filename = QFileDialog::getSaveFileName(
        P->mainwindow_, tr("Save profile"),
        QString(),
        tr("Profile (*.profile)"));

    if (filename.isEmpty())
        return;

    QDir(filename).mkpath(".");

    cfloat *responses[] = {
        P->an_lo_response_.get(),
        P->an_hi_response_.get(),
    };
    bool response_enabled[] = {
        P->lo_enable_,
        P->hi_enable_,
    };
    const char *response_names[] = {
        "lo",
        "hi",
    };

    for (unsigned r = 0; r < 2; ++r) {
        if (!response_enabled[r])
            continue;
        std::ofstream file((filename + "/" + response_names[r] + ".dat").toLocal8Bit().data());
        file << std::scientific << std::setprecision(10);
        for (unsigned i = 0; i < Analysis::sweep_length; ++i) {
            double freq = P->an_freqs_[i];
            cfloat response = responses[r][i];
            file << freq << ' ' << std::abs(response) << ' ' << std::arg(response) << '\n';
        }
        if (!file.flush()) {
            QMessageBox::warning(P->mainwindow_, tr("Output error"), tr("Could not save profile data."));
            return;
        }
    }
}

void Application::realtimeUpdateTick()
{
    Audio_Processor &proc = *P->proc_;

    while (Basic_Message *hmsg = proc.receive_message()) {
        switch (hmsg->tag) {
        case Message_Tag::NotifyFrequencyAnalysis: {
            auto *msg = (Messages::NotifyFrequencyAnalysis *)hmsg;

            int spl = msg->spl;
            if (spl == -1)
                return;

            unsigned index = P->sweep_index_;

            double *an_freqs = P->an_freqs_.get();
            cfloat *response = ((spl == Analysis::Signal_Hi) ?
                                P->an_hi_response_ : P->an_lo_response_).get();

            unsigned done_bins = msg->num_bins;
            for (unsigned a = 0; a < done_bins; ++a)  {
                unsigned dst_index = Analysis::nth_bin_position(index, a, done_bins);

                an_freqs[dst_index] = msg->frequency[a];
                response[dst_index] = msg->response[a];

                double *plot_mags = ((spl == Analysis::Signal_Hi) ?
                                     P->an_hi_plot_mags_ : P->an_lo_plot_mags_).get();
                double *plot_phases = ((spl == Analysis::Signal_Hi) ?
                                       P->an_hi_plot_phases_ : P->an_lo_plot_phases_).get();

                plot_mags[dst_index] = 20 * std::log10(std::abs(msg->response[a]));
                plot_phases[dst_index] = std::arg(msg->response[a]);

                P->sweep_progress_.set(dst_index);
            }

            index = (index + 1) % Analysis::sweep_length;
            if (P->sweep_progress_.count() == Analysis::sweep_length || !P->enabled_spl(spl))
                spl = P->next_spl_phase(P->sweep_spl_);
            P->sweep_index_ = index;
            P->set_sweep_phase(spl);

            P->mainwindow_->showProgress(P->sweep_progress_.count() * (1.0 / Analysis::sweep_length));

            replotResponses();

            if (P->sweep_active_)
                P->tm_nextsweep_->start(0);
            break;
        }
        default:
            assert(false);
            break;
        }
    }

    MainWindow &window = *P->mainwindow_;
    window.showLevels(proc.input_level(), proc.output_level());
}

void Application::nextSweepTick()
{
    Audio_Processor &proc = *P->proc_;
    unsigned index = P->sweep_index_;

    Messages::RequestAnalyzeFrequency msg;
    msg.spl = P->sweep_spl_;
    msg.num_bins = P->freqs_at_once_;
    for (unsigned a = 0; a < msg.num_bins; ++a) {
        unsigned src_index = Analysis::nth_bin_position(index, a, msg.num_bins);
        msg.frequency[a] = P->an_freqs_[src_index];
    }
    proc.send_message(msg);

    P->mainwindow_->showCurrentFrequency(msg.frequency[0]);
}

void Application::replotResponses()
{
    const unsigned ns = Analysis::sweep_length;
    P->mainwindow_->showPlotData
        (P->an_freqs_.get(), P->an_freqs_[P->sweep_index_],
         P->an_lo_plot_mags_.get(), P->an_lo_plot_phases_.get(),
         P->an_hi_plot_mags_.get(), P->an_hi_plot_phases_.get(),
         ns);
}

int Application::Impl::next_spl_phase(int spl) const
{
    if (!lo_enable_ && !hi_enable_)
        return -1;
    else if ((spl == -1 || spl == Analysis::Signal_Hi) && lo_enable_)
        return Analysis::Signal_Lo;
    else if ((spl == -1 || spl == Analysis::Signal_Lo) && hi_enable_)
        return Analysis::Signal_Hi;
    else
        return spl;
}

bool Application::Impl::enabled_spl(int spl) const
{
    switch (spl) {
    default:
        return false;
    case Analysis::Signal_Lo:
        return lo_enable_;
    case Analysis::Signal_Hi:
        return hi_enable_;
    }
}

void Application::Impl::set_sweep_phase(int spl)
{
    if (sweep_spl_ == spl)
        return;
    sweep_spl_ = spl;
    sweep_progress_.reset();
    emit theApplication->sweepPhaseChanged(spl);
}
