CONFIG += testcase
# This testcase can be slow on Windows and may interfere with other file system tests.
win32:testcase.timeout = 900

QT += widgets widgets-private
QT += core-private gui testlib

SOURCES		+= tst_qfilesystemmodel.cpp
TARGET		= tst_qfilesystemmodel
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

mac:CONFIG+=insignificant_test # QTBUG-27890
