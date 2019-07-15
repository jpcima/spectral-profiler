//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <QApplication>
#include <memory>
class Audio_Processor;
class MainWindow;

class Application : public QApplication {
    Q_OBJECT

public:
    Application(int &argc, char *argv[]);
    ~Application();

    void setAudioProcessor(Audio_Processor &proc);
    void setMainWindow(MainWindow &win);

    void setSweepEnabled(bool lo, bool hi);
    void setFreqsAtOnce(unsigned count);

signals:
    void sweepPhaseChanged(int spl);

public slots:
    void setSweepActive(bool active);
    void saveProfile();

protected slots:
    void realtimeUpdateTick();
    void nextSweepTick();

private:
    void replotResponses();

private:
    struct Impl;
    std::unique_ptr<Impl> P;
};

#define theApplication static_cast<::Application *>(qApp)
