QT = widgets
CONFIG += qwt

SOURCES = \
    sources/main.cc \
    sources/application.cc \
    sources/mainwindow.cc \
    sources/audiosys.cc \
    sources/audioprocessor.cc \
    sources/analyzerdefs.cc \
    sources/messages.cc \
    sources/utility/ring_buffer.cpp

HEADERS = \
    sources/application.h \
    sources/mainwindow.h \
    sources/audiosys.h \
    sources/audioprocessor.h \
    sources/analyzerdefs.h \
    sources/messages.h \
    sources/utility/nextpow2.h \
    sources/utility/ring_buffer.h \
    sources/utility/counting_bitset.h \
    sources/utility/counting_bitset.tcc

FORMS = \
    forms/mainwindow.ui

LIBS = -ljack -lfftw3f

DESTDIR = build
OBJECTS_DIR = build/obj
MOC_DIR = build/moc
UI_DIR = build/ui
