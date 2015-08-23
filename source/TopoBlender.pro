QT          += core gui opengl widgets xml

TARGET      = TopoBlender
TEMPLATE    = app

INCLUDEPATH += . external

SOURCES +=  main.cpp\
            mainwindow.cpp \
            GraphicsView.cpp \
            GraphicsScene.cpp \
            ModifiersPanel.cpp \
            Tool.cpp \
            Viewer.cpp \
            Document.cpp \
            Model.cpp \
            ModelMesher.cpp \
# Sketch tool
            Tools/Sketch/Sketch.cpp \
            Tools/Sketch/SketchView.cpp \
            Tools/Sketch/SketchManipulatorTool.cpp \
    Tools/Sketch/SketchDuplicate.cpp \
    ModelConnector.cpp

HEADERS  += mainwindow.h \
            GeometryHelper.h \
            GraphicsView.h \
            GraphicsScene.h \
            ModifiersPanel.h \
            Tool.h \
            Viewer.h \
            Camera.h \
            Document.h \
            Model.h \
            ModelMesher.h \
# Sketch tool
            Tools/Sketch/Sketch.h \
            Tools/Sketch/SketchView.h \
            Tools/Sketch/SketchManipulatorTool.h \
    Tools/Sketch/SketchDuplicate.h \
    ModelConnector.h

# Qt UI files
FORMS    += mainwindow.ui \
            ModifiersPanel.ui \
            Tools/Sketch/Sketch.ui \
    Tools/Sketch/SketchDuplicate.ui

win32{
    # Eigen 3.2.5 introduced some new warnings
    QMAKE_CXXFLAGS *= /wd4522

    # Use native OpenGL drivers with Qt5.5
    # No longer implicit since the ANGLE driver is now an alternative
    LIBS += -lopengl32 -lglu32
}

# C++11 support on linux
linux-g++{ CONFIG += c++11 warn_off }

# OpenMP
win32{
    QMAKE_CXXFLAGS *= /openmp
}
unix:!mac{
    QMAKE_CXXFLAGS *= -fopenmp
    LIBS += -lgomp
}

### GeoTopo Libraries

# Build flag
CONFIG(debug, debug|release) {CFG = debug} else {CFG = release}

# GeoTopo library
LIBS += -L$$PWD/../../GeoTopo/source/GeoTopoLib/lib/$$CFG -lGeoTopoLib
INCLUDEPATH += ../../GeoTopo/source/GeoTopoLib

# StructureGraph library
LIBS += -L$$PWD/../../GeoTopo/source/StructureGraphLib/lib/$$CFG -lStructureGraphLib
INCLUDEPATH += ../../GeoTopo/source/StructureGraphLib

# Surface mesh library
LIBS += -L$$PWD/../../GeoTopo/source/external/SurfaceMesh/lib/$$CFG -lSurfaceMesh
INCLUDEPATH += ../../GeoTopo/source/external/SurfaceMesh ../../GeoTopo/source/external/SurfaceMesh/surface_mesh

# NURBS library
LIBS += -L$$PWD/../../GeoTopo/source/NURBS/lib/$$CFG -lNURBS
INCLUDEPATH += ../../GeoTopo/source/NURBS

# SDF library
INCLUDEPATH += external/SDFGen

linux-g++{ LIBS += -lGLU }
