/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/QtTest>
#include <qdir.h>
#include <qpluginloader.h>
#include "theplugin/plugininterface.h"

// Helper macros to let us know if some suffixes are valid
#define bundle_VALID    false
#define dylib_VALID     false
#define sl_VALID        false
#define a_VALID         false
#define so_VALID        false
#define dll_VALID       false

#if defined(Q_OS_DARWIN)
# undef bundle_VALID
# undef dylib_VALID
# undef so_VALID
# define bundle_VALID   true
# define dylib_VALID    true
# define so_VALID       true
//# ifdef QT_NO_DEBUG
#  define SUFFIX         ".dylib"
//# else
//#  define SUFFIX         "_debug.dylib"
//#endif
# define PREFIX         "lib"

#elif defined(Q_OS_HPUX) && !defined(__ia64)
# undef sl_VALID
# define sl_VALID       true
# define SUFFIX         ".sl"
# define PREFIX         "lib"

#elif defined(Q_OS_AIX)
# undef a_VALID
# undef so_VALID
# define a_VALID        true
# define so_VALID       true
# define SUFFIX         ".so"
# define PREFIX         "lib"

#elif defined(Q_OS_WIN)
# undef dll_VALID
# define dll_VALID      true
//# ifdef QT_NO_DEBUG
#  define SUFFIX         ".dll"
//# else
//#  define SUFFIX         "d.dll"
//# endif
# define PREFIX         ""

#else  // all other Unix
# undef so_VALID
# define so_VALID       true
# define SUFFIX         ".so"
# define PREFIX         "lib"
#endif

static QString sys_qualifiedLibraryName(const QString &fileName)
{
    return QFINDTESTDATA(QString("bin/%1%2%3").arg(PREFIX).arg(fileName).arg(SUFFIX));
}

QT_FORWARD_DECLARE_CLASS(QPluginLoader)
class tst_QPluginLoader : public QObject
{
    Q_OBJECT
private slots:
    void errorString();
    void loadHints();
    void deleteinstanceOnUnload();
    void loadDebugObj();
    void loadCorruptElf();
#if defined (Q_OS_UNIX)
    void loadGarbage();
#endif
    void relativePath();
    void reloadPlugin();
};

void tst_QPluginLoader::errorString()
{
#if defined(Q_OS_WINCE)
    // On WinCE we need an QCoreApplication object for current dir
    int argc = 0;
    QCoreApplication app(argc,0);
#endif
    const QString unknown(QLatin1String("Unknown error"));

    {
    QPluginLoader loader; // default constructed
    bool loaded = loader.load();
    QCOMPARE(loader.errorString(), unknown);
    QVERIFY(!loaded);

    QObject *obj = loader.instance();
    QCOMPARE(loader.errorString(), unknown);
    QCOMPARE(obj, static_cast<QObject*>(0));

    bool unloaded = loader.unload();
    QCOMPARE(loader.errorString(), unknown);
    QVERIFY(!unloaded);
    }
    {
    QPluginLoader loader( sys_qualifiedLibraryName("tst_qpluginloaderlib"));     //not a plugin
    bool loaded = loader.load();
    QVERIFY(loader.errorString() != unknown);
    QVERIFY(!loaded);

    QObject *obj = loader.instance();
    QVERIFY(loader.errorString() != unknown);
    QCOMPARE(obj, static_cast<QObject*>(0));

    bool unloaded = loader.unload();
    QVERIFY(loader.errorString() != unknown);
    QVERIFY(!unloaded);
    }

    {
    QPluginLoader loader( sys_qualifiedLibraryName("nosuchfile"));     //not a file
    bool loaded = loader.load();
    QVERIFY(loader.errorString() != unknown);
    QVERIFY(!loaded);

    QObject *obj = loader.instance();
    QVERIFY(loader.errorString() != unknown);
    QCOMPARE(obj, static_cast<QObject*>(0));

    bool unloaded = loader.unload();
    QVERIFY(loader.errorString() != unknown);
    QVERIFY(!unloaded);
    }

#if !defined Q_OS_WIN && !defined Q_OS_MAC && !defined Q_OS_HPUX
    {
    QPluginLoader loader( sys_qualifiedLibraryName("almostplugin"));     //a plugin with unresolved symbols
    loader.setLoadHints(QLibrary::ResolveAllSymbolsHint);
    bool loaded = loader.load();
    QVERIFY(loader.errorString() != unknown);
    QVERIFY(!loaded);

    QObject *obj = loader.instance();
    QVERIFY(loader.errorString() != unknown);
    QCOMPARE(obj, static_cast<QObject*>(0));

    bool unloaded = loader.unload();
    QVERIFY(loader.errorString() != unknown);
    QVERIFY(!unloaded);
    }
#endif

    {
    QPluginLoader loader( sys_qualifiedLibraryName("theplugin"));     //a plugin
    QCOMPARE(loader.load(), true);
    QCOMPARE(loader.errorString(), unknown);

    QVERIFY(loader.instance() !=  static_cast<QObject*>(0));
    QCOMPARE(loader.errorString(), unknown);

    // Make sure that plugin really works
    PluginInterface* theplugin = qobject_cast<PluginInterface*>(loader.instance());
    QString pluginName = theplugin->pluginName();
    QCOMPARE(pluginName, QLatin1String("Plugin ok"));

    QCOMPARE(loader.unload(), true);
    QCOMPARE(loader.errorString(), unknown);
    }
}

void tst_QPluginLoader::loadHints()
{
    QPluginLoader loader;
    QCOMPARE(loader.loadHints(), (QLibrary::LoadHints)0);   //Do not crash
    loader.setLoadHints(QLibrary::ResolveAllSymbolsHint);
    loader.setFileName( sys_qualifiedLibraryName("theplugin"));     //a plugin
    QCOMPARE(loader.loadHints(), QLibrary::ResolveAllSymbolsHint);
}

void tst_QPluginLoader::deleteinstanceOnUnload()
{
    for (int pass = 0; pass < 2; ++pass) {
        QPluginLoader loader1;
        loader1.setFileName( sys_qualifiedLibraryName("theplugin"));     //a plugin
        if (pass == 0)
            loader1.load(); // not recommended, instance() should do the job.
        PluginInterface *instance1 = qobject_cast<PluginInterface*>(loader1.instance());
        QVERIFY(instance1);
        QCOMPARE(instance1->pluginName(), QLatin1String("Plugin ok"));

        QPluginLoader loader2;
        loader2.setFileName( sys_qualifiedLibraryName("theplugin"));     //a plugin
        if (pass == 0)
            loader2.load(); // not recommended, instance() should do the job.
        PluginInterface *instance2 = qobject_cast<PluginInterface*>(loader2.instance());
        QCOMPARE(instance2->pluginName(), QLatin1String("Plugin ok"));

        QSignalSpy spy1(loader1.instance(), SIGNAL(destroyed()));
        QSignalSpy spy2(loader2.instance(), SIGNAL(destroyed()));
        QVERIFY(spy1.isValid());
        QVERIFY(spy2.isValid());
        if (pass == 0) {
            QCOMPARE(loader2.unload(), false);  // refcount not reached 0, not really unloaded
            QCOMPARE(spy1.count(), 0);
            QCOMPARE(spy2.count(), 0);
        }
        QCOMPARE(instance1->pluginName(), QLatin1String("Plugin ok"));
        QCOMPARE(instance2->pluginName(), QLatin1String("Plugin ok"));
        QVERIFY(loader1.unload());   // refcount reached 0, did really unload
        QCOMPARE(spy1.count(), 1);
        QCOMPARE(spy2.count(), 1);
    }
}

void tst_QPluginLoader::loadDebugObj()
{
#if defined (__ELF__)
    QVERIFY(QFile::exists(QFINDTESTDATA("elftest/debugobj.so")));
    QPluginLoader lib1(QFINDTESTDATA("elftest/debugobj.so"));
    QCOMPARE(lib1.load(), false);
#endif
}

void tst_QPluginLoader::loadCorruptElf()
{
#if defined (__ELF__)
    if (sizeof(void*) == 8) {
        QVERIFY(QFile::exists(QFINDTESTDATA("elftest/corrupt1.elf64.so")));

        QPluginLoader lib1(QFINDTESTDATA("elftest/corrupt1.elf64.so"));
        QCOMPARE(lib1.load(), false);
        QVERIFY2(lib1.errorString().contains("not an ELF object"), qPrintable(lib1.errorString()));

        QPluginLoader lib2(QFINDTESTDATA("elftest/corrupt2.elf64.so"));
        QCOMPARE(lib2.load(), false);
        QVERIFY2(lib2.errorString().contains("invalid"), qPrintable(lib2.errorString()));

        QPluginLoader lib3(QFINDTESTDATA("elftest/corrupt3.elf64.so"));
        QCOMPARE(lib3.load(), false);
        QVERIFY2(lib3.errorString().contains("invalid"), qPrintable(lib3.errorString()));
    } else if (sizeof(void*) == 4) {
        QPluginLoader libW(QFINDTESTDATA("elftest/corrupt3.elf64.so"));
        QCOMPARE(libW.load(), false);
        QVERIFY2(libW.errorString().contains("architecture"), qPrintable(libW.errorString()));
    } else {
        QFAIL("Please port QElfParser to this platform or blacklist this test.");
    }
#endif
}

#if defined (Q_OS_UNIX)
void tst_QPluginLoader::loadGarbage()
{
    for (int i=0; i<5; i++) {
        QPluginLoader lib(QFINDTESTDATA(QString("elftest/garbage%1.so").arg(i+1)));
        QCOMPARE(lib.load(), false);
        QVERIFY(lib.errorString() != QString("Unknown error"));
    }
}
#endif

void tst_QPluginLoader::relativePath()
{
    // Windows binaries run from release and debug subdirs, so we can't rely on the current dir.
    const QString binDir = QFINDTESTDATA("bin");
    QVERIFY(!binDir.isEmpty());
    QCoreApplication::addLibraryPath(binDir);
    QPluginLoader loader("theplugin");
    loader.load(); // not recommended, instance() should do the job.
    PluginInterface *instance = qobject_cast<PluginInterface*>(loader.instance());
    QVERIFY(instance);
    QCOMPARE(instance->pluginName(), QLatin1String("Plugin ok"));
    QVERIFY(loader.unload());
}

void tst_QPluginLoader::reloadPlugin()
{
    QPluginLoader loader;
    loader.setFileName( sys_qualifiedLibraryName("theplugin"));     //a plugin
    loader.load(); // not recommended, instance() should do the job.
    PluginInterface *instance = qobject_cast<PluginInterface*>(loader.instance());
    QVERIFY(instance);
    QCOMPARE(instance->pluginName(), QLatin1String("Plugin ok"));

    QSignalSpy spy(loader.instance(), SIGNAL(destroyed()));
    QVERIFY(spy.isValid());
    QVERIFY(loader.unload());   // refcount reached 0, did really unload
    QCOMPARE(spy.count(), 1);

    // reload plugin
    QVERIFY(loader.load());
    QVERIFY(loader.isLoaded());

    PluginInterface *instance2 = qobject_cast<PluginInterface*>(loader.instance());
    QVERIFY(instance2);
    QCOMPARE(instance2->pluginName(), QLatin1String("Plugin ok"));
}

QTEST_MAIN(tst_QPluginLoader)
#include "tst_qpluginloader.moc"
