#-------------------------------------------------
#
# Project created by QtCreator 2012-07-07T11:14:03
#
#-------------------------------------------------

QT -= core gui

TARGET = property
TEMPLATE = lib

QMAKE_CXXFLAGS += -Wno-unused-result

DEFINES +=

SOURCES += \
    PropertyBase.cpp \
    Property.cpp

HEADERS += \
    PropertyBase.h \
    Property.h \
    PropertyTemplateInstanciations.h

INCLUDEPATH += ../../simulation
DEPENDPATH += ../../simulation

CONFIG(debug, debug|release) {
    DESTDIR = ../../build/debug
} else {
    DESTDIR = .../../build/release 
}

LIBS += -L$$DESTDIR -lsimulation

OTHER_FILES += \
    Android.mk \
    getProp














