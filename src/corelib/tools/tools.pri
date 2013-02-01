# Qt tools module

HEADERS +=  \
        tools/qalgorithms.h \
        tools/qarraydata.h \
        tools/qarraydataops.h \
        tools/qarraydatapointer.h \
        tools/qbitarray.h \
        tools/qbytearray.h \
        tools/qbytearraymatcher.h \
        tools/qbytedata_p.h \
        tools/qcache.h \
        tools/qchar.h \
        tools/qcollator_p.h \
        tools/qcontainerfwd.h \
        tools/qcryptographichash.h \
        tools/qdatetime.h \
        tools/qdatetime_p.h \
        tools/qeasingcurve.h \
        tools/qfreelist_p.h \
        tools/qhash.h \
        tools/qiterator.h \
        tools/qline.h \
        tools/qlinkedlist.h \
        tools/qlist.h \
        tools/qlocale.h \
        tools/qlocale_p.h \
        tools/qlocale_tools_p.h \
        tools/qlocale_data_p.h \
        tools/qmap.h \
        tools/qmargins.h \
        tools/qmessageauthenticationcode.h \
        tools/qcontiguouscache.h \
        tools/qpodlist_p.h \
        tools/qpair.h \
        tools/qpoint.h \
        tools/qqueue.h \
        tools/qrect.h \
        tools/qregexp.h \
        tools/qregularexpression.h \
        tools/qringbuffer_p.h \
        tools/qrefcount.h \
        tools/qscopedpointer.h \
        tools/qscopedpointer_p.h \
        tools/qscopedvaluerollback.h \
        tools/qshareddata.h \
        tools/qsharedpointer.h \
        tools/qsharedpointer_impl.h \
        tools/qset.h \
        tools/qsimd_p.h \
        tools/qsize.h \
        tools/qstack.h \
        tools/qstring.h \
        tools/qstringbuilder.h \
        tools/qstringlist.h \
        tools/qstringmatcher.h \
        tools/qtextboundaryfinder.h \
        tools/qtimeline.h \
        tools/qelapsedtimer.h \
        tools/qunicodetables_p.h \
        tools/qunicodetools_p.h \
        tools/qvarlengtharray.h \
        tools/qvector.h


SOURCES += \
        tools/qarraydata.cpp \
        tools/qbitarray.cpp \
        tools/qbytearray.cpp \
        tools/qbytearraymatcher.cpp \
        tools/qcollator.cpp \
        tools/qcryptographichash.cpp \
        tools/qdatetime.cpp \
        tools/qeasingcurve.cpp \
        tools/qelapsedtimer.cpp \
        tools/qfreelist.cpp \
        tools/qhash.cpp \
        tools/qline.cpp \
        tools/qlinkedlist.cpp \
        tools/qlist.cpp \
        tools/qlocale.cpp \
        tools/qlocale_tools.cpp \
        tools/qpoint.cpp \
        tools/qmap.cpp \
        tools/qmargins.cpp \
        tools/qmessageauthenticationcode.cpp \
        tools/qcontiguouscache.cpp \
        tools/qrect.cpp \
        tools/qregexp.cpp \
        tools/qregularexpression.cpp \
        tools/qrefcount.cpp \
        tools/qshareddata.cpp \
        tools/qsharedpointer.cpp \
        tools/qsimd.cpp \
        tools/qsize.cpp \
        tools/qstring.cpp \
        tools/qstringbuilder.cpp \
        tools/qstringlist.cpp \
        tools/qtextboundaryfinder.cpp \
        tools/qtimeline.cpp \
        tools/qunicodetools.cpp \
        tools/qvector.cpp \
        tools/qvsnprintf.cpp

!nacl:mac: {
    SOURCES += tools/qelapsedtimer_mac.cpp
    OBJECTIVE_SOURCES += tools/qlocale_mac.mm
}
else:unix:SOURCES += tools/qelapsedtimer_unix.cpp tools/qlocale_unix.cpp
else:win32:SOURCES += tools/qelapsedtimer_win.cpp tools/qlocale_win.cpp
else:integrity:SOURCES += tools/qelapsedtimer_unix.cpp tools/qlocale_unix.cpp
else:SOURCES += tools/qelapsedtimer_generic.cpp

contains(QT_CONFIG, zlib) {
    include($$PWD/../../3rdparty/zlib.pri)
    corelib_zlib_headers.files = $$PWD/../../3rdparty/zlib/zconf.h\
                                 $$PWD/../../3rdparty/zlib/zlib.h
    corelib_zlib_headers.path = $$[QT_INSTALL_HEADERS]/QtZlib
    INSTALLS += corelib_zlib_headers
} else {
    include($$PWD/../../3rdparty/zlib_dependency.pri)
}

contains(QT_CONFIG,icu) {
    SOURCES += tools/qlocale_icu.cpp
    DEFINES += QT_USE_ICU
    win32:LIBS_PRIVATE += -licuin -licuuc
    else:LIBS_PRIVATE += -licui18n -licuuc
}

pcre {
    include($$PWD/../../3rdparty/pcre.pri)
} else {
    LIBS_PRIVATE += -lpcre16
}

DEFINES += HB_EXPORT=Q_CORE_EXPORT
HEADERS += ../3rdparty/harfbuzz/src/harfbuzz.h
SOURCES += ../3rdparty/harfbuzz/src/harfbuzz-buffer.c \
           ../3rdparty/harfbuzz/src/harfbuzz-gdef.c \
           ../3rdparty/harfbuzz/src/harfbuzz-gsub.c \
           ../3rdparty/harfbuzz/src/harfbuzz-gpos.c \
           ../3rdparty/harfbuzz/src/harfbuzz-impl.c \
           ../3rdparty/harfbuzz/src/harfbuzz-open.c \
           ../3rdparty/harfbuzz/src/harfbuzz-stream.c \
           ../3rdparty/harfbuzz/src/harfbuzz-shaper-all.cpp \
           tools/qharfbuzz.cpp
HEADERS += tools/qharfbuzz_p.h

INCLUDEPATH += ../3rdparty/md5 \
               ../3rdparty/md4

# Note: libm should be present by default becaue this is C++
!macx-icc:!vxworks:unix:LIBS_PRIVATE += -lm

TR_EXCLUDE += ../3rdparty/*
