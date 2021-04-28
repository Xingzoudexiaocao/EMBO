QT       += core gui network serialport help

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

win32:RC_ICONS = icon.ico
macx: ICON = icon.icns


greaterThan(QT_MAJOR_VERSION, 4){
    TARGET_ARCH=$${QT_ARCH}
}else{
    TARGET_ARCH=$${QMAKE_HOST.arch}
}

CONFIG(release, debug|release): DESTDIR = $$OUT_PWD/release
CONFIG(debug, debug|release): DESTDIR = $$OUT_PWD/debug


include(__crashhandler/qBreakpad.pri)
include(__updater/QSimpleUpdater.pri)

win32 {
    contains(TARGET_ARCH, x86_64) {

        ARCHITECTURE = win64
        QMAKE_LIBDIR += $$PWD/lib/win_64
        LIBS += $$PWD/lib/win_64/libfftw3-3.dll
        LIBS += -lqBreakpad

        inst.files += $$PWD/lib/win_64/libfftw3-3.dll
        inst.path += $${DESTDIR}
        INSTALLS += inst

    } else {

        ARCHITECTURE = win32
        QMAKE_LIBDIR += $$PWD/lib/win_32
        LIBS += $$PWD/lib/win_32/libfftw3-3.dll
        LIBS += -lqBreakpad

        inst.files += $$PWD/lib/win_32/libfftw3-3.dll
        inst.path += $${DESTDIR}
        INSTALLS += inst
    }
}


linux {
    ARCHITECTURE = linux64
    QMAKE_LIBDIR += $$PWD/lib/linux_64
    LIBS += -lfftw3-3
    LIBS += -lqBreakpad
}

macx {
    ARCHITECTURE = mac64
    QMAKE_LIBDIR += $$PWD/lib/mac_64
    INCLUDEPATH += /usr/local/include
    LIBS += -framework AppKit
    LIBS += -lfftw3-3 -lm
    LIBS += -lqBreakpad
}

#LIBS += -lOpenGL32
#DEFINES += QCUSTOMPLOT_USE_OPENGL

fw.files += "$${PWD}/firmware/EMBO_F103C8.hex"
fw.path += $${DESTDIR}/__firmware
INSTALLS += fw

win32 {
    drivers.files += "$${PWD}/drivers/VCP_V1.5.0_Setup_W7_x64_64bits.exe" "$${PWD}/drivers/VCP_V1.5.0_Setup_W7_x86_32bits.exe" "$${PWD}/drivers/VCP_V1.5.0_Setup_W8_x64_64bits.exe" "$${PWD}/drivers/VCP_V1.5.0_Setup_W8_x86_32bits.exe"
    drivers.path += $${DESTDIR}/__drivers
    INSTALLS += drivers
}


VERSION = 0.0.4
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

QMAKE_TARGET_COMPANY = CTU Jakub Parez
QMAKE_TARGET_PRODUCT = EMBO
QMAKE_TARGET_DESCRIPTION = EMBedded Oscilloscope
QMAKE_TARGET_COPYRIGHT = CTU Jakub Parez


# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += src/
INCLUDEPATH += src/windows/

SOURCES += \
    src/core.cpp \
    lib/ctkrangeslider.cpp \
    lib/qcustomplot.cpp \
    src/main.cpp \
    src/messages.cpp \
    src/msg.cpp \
    src/qcpcursors.cpp \
    src/recorder.cpp \
    src/settings.cpp \
    src/utils.cpp \
    src/windows/window__main.cpp \
    src/windows/window_cntr.cpp \
    src/windows/window_la.cpp \
    src/windows/window_pwm.cpp \
    src/windows/window_scope.cpp \
    src/windows/window_sgen.cpp \
    src/windows/window_vm.cpp

HEADERS += \
    src/containers.h \
    src/core.h \
    src/css.h \
    src/interfaces.h \
    lib/ctkrangeslider.h \
    lib/fftw3.h \
    lib/qcustomplot.h \
    src/messages.h \
    src/movemean.h \
    src/msg.h \
    src/qcpcursors.h \
    src/recorder.h \
    src/settings.h \
    src/utils.h \
    src/windows/window__main.h \
    src/windows/window_cntr.h \
    src/windows/window_la.h \
    src/windows/window_pwm.h \
    src/windows/window_scope.h \
    src/windows/window_sgen.h \
    src/windows/window_vm.h

FORMS += \
    src/windows/window__main.ui \
    src/windows/window_cntr.ui \
    src/windows/window_la.ui \
    src/windows/window_pwm.ui \
    src/windows/window_scope.ui \
    src/windows/window_sgen.ui \
    src/windows/window_vm.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += resources/resources.qrc

DISTFILES += \
    icon.icns \
    icon.ico
