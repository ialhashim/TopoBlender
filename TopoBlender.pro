QT          += core gui opengl widgets

TARGET      = FancyViewer
TEMPLATE    = app

INCLUDEPATH += . external

SOURCES +=  main.cpp\
            mainwindow.cpp \
            GraphicsView.cpp \
            GraphicsScene.cpp \
            ModifiersPanel.cpp \
            Tool.cpp \
            Viewer.cpp


HEADERS  += mainwindow.h \
            GraphicsView.h \
            GraphicsScene.h \
            ModifiersPanel.h \
            Tool.h \
            Viewer.h \
            Camera.h

# Sketch tool
SOURCES +=  Tools/Sketch/Sketch.cpp \
            Tools/Sketch/SketchView.cpp

HEADERS  += Tools/Sketch/Sketch.h \
            Tools/Sketch/SketchView.h

# Qt UI files
FORMS    += mainwindow.ui \
            ModifiersPanel.ui

# Eigen 3.2.5 introduced some new warnings
win32:QMAKE_CXXFLAGS *= /wd4522
