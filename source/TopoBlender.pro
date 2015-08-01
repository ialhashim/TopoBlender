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
            Model.cpp

HEADERS  += mainwindow.h \
            GraphicsView.h \
            GraphicsScene.h \
            ModifiersPanel.h \
            Tool.h \
            Viewer.h \
            Camera.h \
            Document.h \
            Model.h

# Sketch tool
SOURCES +=  Tools/Sketch/Sketch.cpp \
            Tools/Sketch/SketchView.cpp

HEADERS  += Tools/Sketch/Sketch.h \
            Tools/Sketch/SketchView.h

# Qt UI files
FORMS    += mainwindow.ui \
            ModifiersPanel.ui

win32{
    # Eigen 3.2.5 introduced some new warnings
    QMAKE_CXXFLAGS *= /wd4522

    # Use native OpenGL drivers with Qt5.5
    # No longer implicit since the ANGLE driver is now an alternative
    LIBS += -lopengl32 -lglu32
}

### GeoTopo Libraries

# Build flag
CONFIG(debug, debug|release) {CFG = debug} else {CFG = release}

# NURBS library
LIBS += -L$$PWD/../../GeoTopo/source/NURBS/lib/$$CFG -lNURBS
INCLUDEPATH += ../../GeoTopo/source/NURBS

# Surface mesh library
LIBS += -L$$PWD/../../GeoTopo/source/external/SurfaceMesh/lib/$$CFG -lSurfaceMesh
INCLUDEPATH += ../../GeoTopo/source/external/SurfaceMesh ../../GeoTopo/source/external/SurfaceMesh/surface_mesh

# StructureGraph library
LIBS += -L$$PWD/../../GeoTopo/source/StructureGraphLib/lib/$$CFG -lStructureGraphLib
INCLUDEPATH += ../../GeoTopo/source/StructureGraphLib

# GeoTopo library
LIBS += -L$$PWD/../../GeoTopo/source/GeoTopoLib/lib/$$CFG -lGeoTopoLib
INCLUDEPATH += ../../GeoTopo/source/GeoTopoLib
