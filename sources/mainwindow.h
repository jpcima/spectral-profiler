//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <QMainWindow>
#include <memory>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void showCurrentFrequency(float f);
    void showLevels(float in, float out);
    void showProgress(float progress);
    void showPlotData(
        const double *freqs, double freqmark,
        const double *lo_mags, const double *lo_phases,
        const double *hi_mags, const double *hi_phases, unsigned n);

private:
    struct Impl;
    std::unique_ptr<Impl> P;
};
