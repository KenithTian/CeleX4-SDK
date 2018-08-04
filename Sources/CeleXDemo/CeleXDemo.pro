#-------------------------------------------------
#
# Project created by QtCreator 2018-02-24T14:04:33
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = CeleXDemo
TEMPLATE = app

win32 {
    INCLUDEPATH += D:/opencv/build/include \
                   D:/Program Files/opencv/build/include/opencv \
                   D:/Program Files/opencv/build/include/opencv2
}
else {

    INCLUDEPATH += /usr/local/include \
                   /usr/local/include/opencv \
                   /usr/local/include/opencv2

    LIBS += /usr/local/lib/libopencv_highgui.so \
            /usr/local/lib/libopencv_core.so    \
            /usr/local/lib/libopencv_imgproc.so \
            /usr/local/lib/libopencv_videoio.so
}

win32 {
    contains(QT_ARCH, i386) {
        CONFIG(release, debug|release): LIBS += -L$$PWD/lib/Windows/x86/release/ -lCeleX -lopencv_world330
        else:CONFIG(debug, debug|release): LIBS += -L$$PWD/lib/Windows/x86/debug/ -lCeleX -lopencv_world330d
    }
    else {
        CONFIG(release, debug|release): LIBS += -L$$PWD/lib/Windows/x64/release/ -lCeleX -lopencv_world330
        else:CONFIG(debug, debug|release): LIBS += -L$$PWD/lib/Windows/x64/debug/ -lCeleX -lopencv_world330d
    }
}
else {
    contains(QT_ARCH, i386) {
        LIBS += -L$$PWD/lib/Linux/x86 -lokFrontPanel -lCeleX }
    else {
        LIBS += -L$$PWD/lib/Linux/x64 -lokFrontPanel -lCeleX }
}

SOURCES += main.cpp\
    mainwindow.cpp \
    dataqueue.cpp \
    doubleslider.cpp \
    hhsliderwidget.cpp

HEADERS  += mainwindow.h \
    okFrontPanelDLL.h \
    dataqueue.h \
    doubleslider.h \
    hhsliderwidget.h \
    hhconstants.h \
    ui_mainwindow.h \

FORMS += \
    mainwindow.ui

win32 {
    CONFIG(debug) {
        QMAKE_POST_LINK += copy $$shell_path($$PWD)\*.xml $$shell_path($$OUT_PWD)\debug\ &
        QMAKE_POST_LINK += copy $$shell_path($$PWD)\*.dll $$shell_path($$OUT_PWD)\debug\ &
        QMAKE_POST_LINK += copy $$shell_path($$PWD)\FPN.txt $$shell_path($$OUT_PWD)\debug\ &
        QMAKE_POST_LINK += copy $$shell_path($$PWD)\*.bit $$shell_path($$OUT_PWD)\debug\ &
        QMAKE_POST_LINK += copy $$shell_path($$PWD)\*.xml $$shell_path($$OUT_PWD)\release\ &
        QMAKE_POST_LINK += copy $$shell_path($$PWD)\*.dll $$shell_path($$OUT_PWD)\release\ &
        QMAKE_POST_LINK += copy $$shell_path($$PWD)\FPN.txt $$shell_path($$OUT_PWD)\release\ &
        QMAKE_POST_LINK += copy $$shell_path($$PWD)\*.bit $$shell_path($$OUT_PWD)\release\
    }
    else {
        QMAKE_POST_LINK += copy $$shell_path($$PWD)\*.xml $$shell_path($$OUT_PWD)\debug\ &
        QMAKE_POST_LINK += copy $$shell_path($$PWD)\*.dll $$shell_path($$OUT_PWD)\debug\ &
        QMAKE_POST_LINK += copy $$shell_path($$PWD)\FPN.txt $$shell_path($$OUT_PWD)\debug\ &
        QMAKE_POST_LINK += copy $$shell_path($$PWD)\*.bit $$shell_path($$OUT_PWD)\debug\ &
        QMAKE_POST_LINK += copy $$shell_path($$PWD)\*.xml $$shell_path($$OUT_PWD)\release\ &
        QMAKE_POST_LINK += copy $$shell_path($$PWD)\*.dll $$shell_path($$OUT_PWD)\release\ &
        QMAKE_POST_LINK += copy $$shell_path($$PWD)\FPN.txt $$shell_path($$OUT_PWD)\release\ &
        QMAKE_POST_LINK += copy $$shell_path($$PWD)\*.bit $$shell_path($$OUT_PWD)\release\
    }
}
else {
    QMAKE_POST_LINK += cp $$shell_path($$PWD)/*.xml $$shell_path($$OUT_PWD)/ &
    QMAKE_POST_LINK += cp $$shell_path($$PWD)/FPN.txt $$shell_path($$OUT_PWD)/ &
    QMAKE_POST_LINK += cp $$shell_path($$PWD)/*.bit $$shell_path($$OUT_PWD)/
}

RESOURCES += \
    images.qrc

