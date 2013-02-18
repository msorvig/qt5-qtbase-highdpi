/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qwindowsintegration.h"
#include "qwindowsbackingstore.h"
#include "qwindowswindow.h"
#include "qwindowscontext.h"
#if defined(QT_OPENGL_ES_2)
#  include "qwindowseglcontext.h"
#  include <QtGui/QOpenGLContext>
#elif !defined(QT_NO_OPENGL)
#  include "qwindowsglcontext.h"
#endif
#include "qwindowsscreen.h"
#include "qwindowstheme.h"
#include "qwindowsservices.h"
#ifndef QT_NO_FREETYPE
#  include "qwindowsfontdatabase_ft.h"
#endif
#include "qwindowsfontdatabase.h"
#include "qwindowsguieventdispatcher.h"
#ifndef QT_NO_CLIPBOARD
#  include "qwindowsclipboard.h"
#  ifndef QT_NO_DRAGANDDROP
#    include "qwindowsdrag.h"
#  endif
#endif
#include "qwindowsinputcontext.h"
#include "qwindowskeymapper.h"
#  ifndef QT_NO_ACCESSIBILITY
#include "accessible/qwindowsaccessibility.h"
#endif

#include <qpa/qplatformnativeinterface.h>
#include <qpa/qwindowsysteminterface.h>
#include <QtGui/QBackingStore>
#include <QtGui/private/qpixmap_raster_p.h>
#include <QtGui/private/qguiapplication_p.h>

#include <QtCore/private/qeventdispatcher_win_p.h>
#include <QtCore/QDebug>
#include <QtCore/QVariant>

QT_BEGIN_NAMESPACE

/*!
    \class QWindowsNativeInterface
    \brief Provides access to native handles.

    Currently implemented keys
    \list
    \li handle (HWND)
    \li getDC (DC)
    \li releaseDC Releases the previously acquired DC and returns 0.
    \endlist

    \internal
    \ingroup qt-lighthouse-win
*/

class QWindowsNativeInterface : public QPlatformNativeInterface
{
    Q_OBJECT
    Q_PROPERTY(bool asyncExpose READ asyncExpose WRITE setAsyncExpose)
public:
#ifndef QT_NO_OPENGL
    virtual void *nativeResourceForContext(const QByteArray &resource, QOpenGLContext *context);
#endif
    virtual void *nativeResourceForWindow(const QByteArray &resource, QWindow *window);
    virtual void *nativeResourceForBackingStore(const QByteArray &resource, QBackingStore *bs);

    Q_INVOKABLE void *createMessageWindow(const QString &classNameTemplate,
                                          const QString &windowName,
                                          void *eventProc) const;

    Q_INVOKABLE QString registerWindowClass(const QString &classNameIn, void *eventProc) const;

    bool asyncExpose() const;
    void setAsyncExpose(bool value);

    QVariantMap windowProperties(QPlatformWindow *window) const;
    QVariant windowProperty(QPlatformWindow *window, const QString &name) const;
    QVariant windowProperty(QPlatformWindow *window, const QString &name, const QVariant &defaultValue) const;
    void setWindowProperty(QPlatformWindow *window, const QString &name, const QVariant &value);
};

void *QWindowsNativeInterface::nativeResourceForWindow(const QByteArray &resource, QWindow *window)
{
    if (!window || !window->handle()) {
        qWarning("%s: '%s' requested for null window or window without handle.", __FUNCTION__, resource.constData());
        return 0;
    }
    QWindowsWindow *bw = static_cast<QWindowsWindow *>(window->handle());
    if (resource == "handle")
        return bw->handle();
    if (window->surfaceType() == QWindow::RasterSurface) {
        if (resource == "getDC")
            return bw->getDC();
        if (resource == "releaseDC") {
            bw->releaseDC();
            return 0;
        }
    }
    qWarning("%s: Invalid key '%s' requested.", __FUNCTION__, resource.constData());
    return 0;
}

void *QWindowsNativeInterface::nativeResourceForBackingStore(const QByteArray &resource, QBackingStore *bs)
{
    if (!bs || !bs->handle()) {
        qWarning("%s: '%s' requested for null backingstore or backingstore without handle.", __FUNCTION__, resource.constData());
        return 0;
    }
    QWindowsBackingStore *wbs = static_cast<QWindowsBackingStore *>(bs->handle());
    if (resource == "getDC")
        return wbs->getDC();
    qWarning("%s: Invalid key '%s' requested.", __FUNCTION__, resource.constData());
    return 0;
}

static const char customMarginPropertyC[] = "WindowsCustomMargins";

QVariant QWindowsNativeInterface::windowProperty(QPlatformWindow *window, const QString &name) const
{
    QWindowsWindow *platformWindow = static_cast<QWindowsWindow *>(window);
    if (name == QLatin1String(customMarginPropertyC))
        return qVariantFromValue(platformWindow->customMargins());
    return QVariant();
}

QVariant QWindowsNativeInterface::windowProperty(QPlatformWindow *window, const QString &name, const QVariant &defaultValue) const
{
    const QVariant result = windowProperty(window, name);
    return result.isValid() ? result : defaultValue;
}

void QWindowsNativeInterface::setWindowProperty(QPlatformWindow *window, const QString &name, const QVariant &value)
{
    QWindowsWindow *platformWindow = static_cast<QWindowsWindow *>(window);
    if (name == QLatin1String(customMarginPropertyC))
        platformWindow->setCustomMargins(qvariant_cast<QMargins>(value));
}

QVariantMap QWindowsNativeInterface::windowProperties(QPlatformWindow *window) const
{
    QVariantMap result;
    const QString customMarginProperty = QLatin1String(customMarginPropertyC);
    result.insert(customMarginProperty, windowProperty(window, customMarginProperty));
    return result;
}

#ifndef QT_NO_OPENGL
void *QWindowsNativeInterface::nativeResourceForContext(const QByteArray &resource, QOpenGLContext *context)
{
    if (!context || !context->handle()) {
        qWarning("%s: '%s' requested for null context or context without handle.", __FUNCTION__, resource.constData());
        return 0;
    }
#ifdef QT_OPENGL_ES_2
    QWindowsEGLContext *windowsEglContext = static_cast<QWindowsEGLContext *>(context->handle());
    if (resource == QByteArrayLiteral("eglDisplay"))
        return windowsEglContext->eglDisplay();
    if (resource == QByteArrayLiteral("eglContext"))
        return windowsEglContext->eglContext();
    if (resource == QByteArrayLiteral("eglConfig"))
        return windowsEglContext->eglConfig();
#else // QT_OPENGL_ES_2
    QWindowsGLContext *windowsContext = static_cast<QWindowsGLContext *>(context->handle());
    if (resource == QByteArrayLiteral("renderingContext"))
        return windowsContext->renderingContext();
#endif // !QT_OPENGL_ES_2

    qWarning("%s: Invalid key '%s' requested.", __FUNCTION__, resource.constData());
    return 0;
}
#endif // !QT_NO_OPENGL

/*!
    \brief Creates a non-visible window handle for filtering messages.
*/

void *QWindowsNativeInterface::createMessageWindow(const QString &classNameTemplate,
                                                   const QString &windowName,
                                                   void *eventProc) const
{
    QWindowsContext *ctx = QWindowsContext::instance();
    const HWND hwnd = ctx->createDummyWindow(classNameTemplate,
                                             (wchar_t*)windowName.utf16(),
                                             (WNDPROC)eventProc);
    return hwnd;
}

/*!
    \brief Registers a unique window class with a callback function based on \a classNameIn.
*/

QString QWindowsNativeInterface::registerWindowClass(const QString &classNameIn, void *eventProc) const
{
    return QWindowsContext::instance()->registerWindowClass(classNameIn, (WNDPROC)eventProc);
}

bool QWindowsNativeInterface::asyncExpose() const
{
    return QWindowsContext::instance()->asyncExpose();
}

void QWindowsNativeInterface::setAsyncExpose(bool value)
{
    QWindowsContext::instance()->setAsyncExpose(value);
}

/*!
    \class QWindowsIntegration
    \brief QPlatformIntegration implementation for Windows.
    \internal

    \section1 Programming Considerations

    The platform plugin should run on Desktop Windows from Windows XP onwards
    and Windows Embedded.

    It should compile with:
    \list
    \li Microsoft Visual Studio 2008 or later (using the Microsoft Windows SDK,
        (\c Q_CC_MSVC).
    \li Stock \l{http://mingw.org/}{MinGW} (\c Q_CC_MINGW).
        This version ships with headers that are missing a lot of WinAPI.
    \li MinGW distributions using GCC 4.7 or higher and a recent MinGW-w64 runtime API,
        such as \l{http://tdm-gcc.tdragon.net/}{TDM-GCC}, or
        \l{http://mingwbuilds.sourceforge.net/}{MinGW-builds}
        (\c Q_CC_MINGW and \c __MINGW64_VERSION_MAJOR indicating the version).
        MinGW-w64 provides more complete headers (compared to stock MinGW from mingw.org),
        including a considerable part of the Windows SDK.
    \li Visual Studio 2008 for Windows Embedded (\c Q_OS_WINCE).
    \endlist

    The file \c qtwindows_additional.h contains defines and declarations that
    are missing in MinGW. When encountering missing declarations, it should
    be added there so that \c #ifdefs for MinGW can be avoided. Similarly,
    \c qplatformfunctions_wince.h contains defines and declarations for
    Windows Embedded.

    When using a function from the WinAPI, the minimum supported Windows version
    and Windows Embedded support should be checked. If the function is not supported
    on Windows XP or is not present in the MinGW-headers, it should be dynamically
    resolved. For this purpose, QWindowsContext has static structs like
    QWindowsUser32DLL and QWindowsShell32DLL. All function pointers should go to
    these structs to avoid lookups in several places.

    \ingroup qt-lighthouse-win
*/

struct QWindowsIntegrationPrivate
{
#if defined(QT_OPENGL_ES_2)
    typedef QSharedPointer<QWindowsEGLStaticContext> QEGLStaticContextPtr;
#elif !defined(QT_NO_OPENGL)
    typedef QSharedPointer<QOpenGLStaticContext> QOpenGLStaticContextPtr;
#endif

    explicit QWindowsIntegrationPrivate(const QStringList &paramList);
    ~QWindowsIntegrationPrivate();

    const unsigned m_options;
    QWindowsContext m_context;
    QPlatformFontDatabase *m_fontDatabase;
    QWindowsNativeInterface m_nativeInterface;
#ifndef QT_NO_CLIPBOARD
    QWindowsClipboard m_clipboard;
#  ifndef QT_NO_DRAGANDDROP
    QWindowsDrag m_drag;
#  endif
#endif
    QWindowsGuiEventDispatcher *m_eventDispatcher;
#if defined(QT_OPENGL_ES_2)
    QEGLStaticContextPtr m_staticEGLContext;
#elif !defined(QT_NO_OPENGL)
    QOpenGLStaticContextPtr m_staticOpenGLContext;
#endif
    QWindowsInputContext m_inputContext;
#ifndef QT_NO_ACCESSIBILITY
    QWindowsAccessibility m_accessibility;
#endif
    QWindowsServices m_services;
};

static inline unsigned parseOptions(const QStringList &paramList)
{
    unsigned options = 0;
    foreach (const QString &param, paramList) {
        if (param.startsWith(QLatin1String("fontengine="))) {
            if (param.endsWith(QLatin1String("freetype"))) {
                options |= QWindowsIntegration::FontDatabaseFreeType;
            } else if (param.endsWith(QLatin1String("native"))) {
                options |= QWindowsIntegration::FontDatabaseNative;
            }
        } else if (param.startsWith(QLatin1String("dialogs="))) {
            if (param.endsWith(QLatin1String("xp"))) {
                options |= QWindowsIntegration::XpNativeDialogs;
            } else if (param.endsWith(QLatin1String("none"))) {
                options |= QWindowsIntegration::NoNativeDialogs;
            }
        } else if (param == QLatin1String("gl=gdi")) {
            options |= QWindowsIntegration::DisableArb;
        }
    }
    return options;
}

QWindowsIntegrationPrivate::QWindowsIntegrationPrivate(const QStringList &paramList)
    : m_options(parseOptions(paramList))
    , m_fontDatabase(0)
    , m_eventDispatcher(new QWindowsGuiEventDispatcher)
{
}

QWindowsIntegrationPrivate::~QWindowsIntegrationPrivate()
{
    if (m_fontDatabase)
        delete m_fontDatabase;
}

QWindowsIntegration::QWindowsIntegration(const QStringList &paramList) :
    d(new QWindowsIntegrationPrivate(paramList))
{
    QGuiApplicationPrivate::instance()->setEventDispatcher(d->m_eventDispatcher);
#ifndef QT_NO_CLIPBOARD
    d->m_clipboard.registerViewer();
#endif
    d->m_context.screenManager().handleScreenChanges();
}

QWindowsIntegration::~QWindowsIntegration()
{
    if (QWindowsContext::verboseIntegration)
        qDebug("%s", __FUNCTION__);
}

bool QWindowsIntegration::hasCapability(QPlatformIntegration::Capability cap) const
{
    switch (cap) {
    case ThreadedPixmaps:
        return true;
#ifndef QT_NO_OPENGL
    case OpenGL:
        return true;
    case ThreadedOpenGL:
#  ifdef QT_OPENGL_ES_2
        return QWindowsEGLContext::hasThreadedOpenGLCapability();
#  else
        return true;
#  endif // QT_OPENGL_ES_2
#endif // !QT_NO_OPENGL
    case WindowMasks:
        return true;
    case MultipleWindows:
        return true;
    default:
        return QPlatformIntegration::hasCapability(cap);
    }
    return false;
}

QPlatformPixmap *QWindowsIntegration::createPlatformPixmap(QPlatformPixmap::PixelType type) const
{
    return new QRasterPlatformPixmap(type);
}

QPlatformWindow *QWindowsIntegration::createPlatformWindow(QWindow *window) const
{
    QWindowsWindow::WindowData requested;
    requested.flags = window->flags();
    requested.geometry = window->geometry();
    // Apply custom margins (see  QWindowsWindow::setCustomMargins())).
    const QVariant customMarginsV = window->property("_q_windowsCustomMargins");
    if (customMarginsV.isValid())
        requested.customMargins = qvariant_cast<QMargins>(customMarginsV);

    const QWindowsWindow::WindowData obtained
            = QWindowsWindow::WindowData::create(window, requested, window->title());
    if (QWindowsContext::verboseIntegration || QWindowsContext::verboseWindows)
        qDebug().nospace()
            << __FUNCTION__ << '<' << window << '\n'
            << "    Requested: " << requested.geometry << "frame incl.: "
            << QWindowsGeometryHint::positionIncludesFrame(window)
            <<   " Flags="
            << QWindowsWindow::debugWindowFlags(requested.flags) << '\n'
            << "    Obtained : " << obtained.geometry << " Margins "
            << obtained.frame  << " Flags="
            << QWindowsWindow::debugWindowFlags(obtained.flags)
            << " Handle=" << obtained.hwnd << '\n';
    if (!obtained.hwnd)
        return 0;
    if (requested.flags != obtained.flags)
        window->setFlags(obtained.flags);
    return new QWindowsWindow(window, obtained);
}

QPlatformBackingStore *QWindowsIntegration::createPlatformBackingStore(QWindow *window) const
{
    if (QWindowsContext::verboseIntegration)
        qDebug() << __FUNCTION__ << window;
    return new QWindowsBackingStore(window);
}

#ifndef QT_NO_OPENGL
QPlatformOpenGLContext
    *QWindowsIntegration::createPlatformOpenGLContext(QOpenGLContext *context) const
{
    if (QWindowsContext::verboseIntegration)
        qDebug() << __FUNCTION__ << context->format();
#ifdef QT_OPENGL_ES_2
    if (d->m_staticEGLContext.isNull()) {
        QWindowsEGLStaticContext *staticContext = QWindowsEGLStaticContext::create();
        if (!staticContext)
            return 0;
        d->m_staticEGLContext = QSharedPointer<QWindowsEGLStaticContext>(staticContext);
    }
    return new QWindowsEGLContext(d->m_staticEGLContext, context->format(), context->shareHandle());
#else  // QT_OPENGL_ES_2
    if (d->m_staticOpenGLContext.isNull())
        d->m_staticOpenGLContext =
            QSharedPointer<QOpenGLStaticContext>(QOpenGLStaticContext::create());
    QScopedPointer<QWindowsGLContext> result(new QWindowsGLContext(d->m_staticOpenGLContext, context));
    if (result->isValid())
        return result.take();
    return 0;
#endif // !QT_OPENGL_ES_2
}
#endif // !QT_NO_OPENGL

/* Workaround for QTBUG-24205: In 'Auto', pick the FreeType engine for
 * QML2 applications. */

#ifdef Q_OS_WINCE
// It's not easy to detect if we are running a QML application
// Let's try to do so by checking if the Qt Quick module is loaded.
inline bool isQMLApplication()
{
    // check if the Qt Quick module is loaded
#ifdef _DEBUG
    HMODULE handle = GetModuleHandle(L"Qt5Quick" QT_LIBINFIX L"d.dll");
#else
    HMODULE handle = GetModuleHandle(L"Qt5Quick" QT_LIBINFIX L".dll");
#endif
    return (handle != NULL);
}
#endif

QPlatformFontDatabase *QWindowsIntegration::fontDatabase() const
{
    if (!d->m_fontDatabase) {
#ifdef QT_NO_FREETYPE
        d->m_fontDatabase = new QWindowsFontDatabase();
#else // QT_NO_FREETYPE
        if (d->m_options & QWindowsIntegration::FontDatabaseFreeType) {
            d->m_fontDatabase = new QWindowsFontDatabaseFT;
        } else if (d->m_options & QWindowsIntegration::FontDatabaseNative){
            d->m_fontDatabase = new QWindowsFontDatabase;
        } else {
#ifndef Q_OS_WINCE
            d->m_fontDatabase = new QWindowsFontDatabase;
#else
            if (isQMLApplication()) {
                if (QWindowsContext::verboseIntegration) {
                    qDebug() << "QML application detected, using FreeType rendering";
                }
                d->m_fontDatabase = new QWindowsFontDatabaseFT;
            }
            else
                d->m_fontDatabase = new QWindowsFontDatabase;
#endif
        }
#endif // QT_NO_FREETYPE
    }
    return d->m_fontDatabase;
}

#ifdef SPI_GETKEYBOARDSPEED
static inline int keyBoardAutoRepeatRateMS()
{
  DWORD time = 0;
  if (SystemParametersInfo(SPI_GETKEYBOARDSPEED, 0, &time, 0))
      return time ? 1000 / static_cast<int>(time) : 500;
  return 30;
}
#endif

QVariant QWindowsIntegration::styleHint(QPlatformIntegration::StyleHint hint) const
{
    switch (hint) {
    case QPlatformIntegration::CursorFlashTime:
        if (const unsigned timeMS = GetCaretBlinkTime())
            return QVariant(int(timeMS));
        break;
#ifdef SPI_GETKEYBOARDSPEED
    case KeyboardAutoRepeatRate:
        return QVariant(keyBoardAutoRepeatRateMS());
#endif
    case QPlatformIntegration::StartDragTime:
    case QPlatformIntegration::StartDragDistance:
    case QPlatformIntegration::KeyboardInputInterval:
    case QPlatformIntegration::ShowIsFullScreen:
    case QPlatformIntegration::PasswordMaskDelay:
    case QPlatformIntegration::StartDragVelocity:
    case QPlatformIntegration::SynthesizeMouseFromTouchEvents:
        break; // Not implemented
    case QPlatformIntegration::FontSmoothingGamma:
        return QVariant(QWindowsFontDatabase::fontSmoothingGamma());
    case QPlatformIntegration::MouseDoubleClickInterval:
        if (const int ms = GetDoubleClickTime())
            return QVariant(ms);
        break;
    case QPlatformIntegration::UseRtlExtensions:
        return QVariant(d->m_context.useRTLExtensions());
    }
    return QPlatformIntegration::styleHint(hint);
}

Qt::KeyboardModifiers QWindowsIntegration::queryKeyboardModifiers() const
{
    return QWindowsKeyMapper::queryKeyboardModifiers();
}

QList<int> QWindowsIntegration::possibleKeys(const QKeyEvent *e) const
{
    return d->m_context.possibleKeys(e);
}

QPlatformNativeInterface *QWindowsIntegration::nativeInterface() const
{
    return &d->m_nativeInterface;
}

#ifndef QT_NO_CLIPBOARD
QPlatformClipboard * QWindowsIntegration::clipboard() const
{
    return &d->m_clipboard;
}
#  ifndef QT_NO_DRAGANDDROP
QPlatformDrag *QWindowsIntegration::drag() const
{
    return &d->m_drag;
}
#  endif // !QT_NO_DRAGANDDROP
#endif // !QT_NO_CLIPBOARD

QPlatformInputContext * QWindowsIntegration::inputContext() const
{
    return &d->m_inputContext;
}

#ifndef QT_NO_ACCESSIBILITY
QPlatformAccessibility *QWindowsIntegration::accessibility() const
{
    return &d->m_accessibility;
}
#endif

QWindowsIntegration *QWindowsIntegration::instance()
{
    return static_cast<QWindowsIntegration *>(QGuiApplicationPrivate::platformIntegration());
}

unsigned QWindowsIntegration::options() const
{
    return d->m_options;
}

QAbstractEventDispatcher * QWindowsIntegration::guiThreadEventDispatcher() const
{
    return d->m_eventDispatcher;
}

QStringList QWindowsIntegration::themeNames() const
{
    return QStringList(QLatin1String(QWindowsTheme::name));
}

QPlatformTheme *QWindowsIntegration::createPlatformTheme(const QString &name) const
{
    if (name == QLatin1String(QWindowsTheme::name))
        return new QWindowsTheme;
    return QPlatformIntegration::createPlatformTheme(name);
}

QPlatformServices *QWindowsIntegration::services() const
{
    return &d->m_services;
}

QT_END_NAMESPACE

#include "qwindowsintegration.moc"
