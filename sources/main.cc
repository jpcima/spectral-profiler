//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "application.h"
#include "mainwindow.h"
#include "audiosys.h"
#include "audioprocessor.h"
#include "analyzerdefs.h"
#include <QMessageBox>

int main(int argc, char *argv[])
{
    Application app(argc, argv);

    Audio_Sys &sys = Audio_Sys::instance();
    if (!sys) {
        QMessageBox::warning(nullptr, app.tr("Error"), app.tr("Cannot start the JACK audio system"));
        return 1;
    }

    Analysis::sample_rate = sys.sample_rate();

    Audio_Processor proc;
    app.setAudioProcessor(proc);
    proc.start();

    MainWindow window;
    app.setMainWindow(window);
    window.show();

    int code = app.exec();
    sys.stop();
    return code;
}
