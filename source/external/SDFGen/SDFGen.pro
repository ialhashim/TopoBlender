TEMPLATE = lib
CONFIG += staticlib

# Build flag
CONFIG(debug, debug|release) {CFG = debug} else {CFG = release}

# Library name and destination
TARGET = SDFGen
DESTDIR = $$PWD/lib/$$CFG

SOURCES += makelevelset3.cpp

HEADERS += \ 
    array1.h \
    array2.h \
    array3.h \
    hashgrid.h \
    hashtable.h \
    makelevelset3.h \
    util.h \
    vec.h
