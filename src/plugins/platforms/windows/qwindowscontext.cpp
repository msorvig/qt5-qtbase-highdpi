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

#include "qwindowscontext.h"
#include "qwindowswindow.h"
#include "qwindowskeymapper.h"
#include "qwindowsguieventdispatcher.h"
#include "qwindowsmousehandler.h"
#include "qtwindowsglobal.h"
#include "qwindowsmime.h"
#include "qwindowsinputcontext.h"
#ifndef QT_NO_ACCESSIBILITY
#include "accessible/qwindowsaccessibility.h"
#endif
#include "qwindowsscreen.h"
#include "qwindowstheme.h"

#include <QtGui/QWindow>
#include <qpa/qwindowsysteminterface.h>
#include <qpa/qplatformnativeinterface.h>
#include <QtGui/QGuiApplication>

#include <QtCore/QSet>
#include <QtCore/QHash>
#include <QtCore/QStringList>
#include <QtCore/QDebug>
#include <QtCore/QSysInfo>
#include <QtCore/QScopedArrayPointer>
#include <QtCore/private/qsystemlibrary_p.h>

#include <stdlib.h>
#include <stdio.h>
#include <windowsx.h>

QT_BEGIN_NAMESPACE

// Verbosity of components
int QWindowsContext::verboseIntegration = 0;
int QWindowsContext::verboseWindows = 0;
int QWindowsContext::verboseEvents = 0;
int QWindowsContext::verboseBackingStore = 0;
int QWindowsContext::verboseFonts = 0;
int QWindowsContext::verboseGL = 0;
int QWindowsContext::verboseOLE = 0;
int QWindowsContext::verboseInputMethods = 0;
int QWindowsContext::verboseDialogs = 0;
int QWindowsContext::verboseTheming = 0;

// Get verbosity of components from "foo:2,bar:3"
static inline int componentVerbose(const char *v, const char *keyWord)
{
    if (const char *k = strstr(v, keyWord)) {
        k += qstrlen(keyWord);
        if (*k == ':') {
            ++k;
            if (isdigit(*k))
                return *k - '0';
        }
    }
    return 0;
}

static inline bool hasTouchSupport(QSysInfo::WinVersion wv)
{
    enum { QT_SM_DIGITIZER = 94, QT_NID_INTEGRATED_TOUCH = 0x1,
           QT_NID_EXTERNAL_TOUCH = 0x02, QT_NID_MULTI_INPUT = 0x40 };

    return wv < QSysInfo::WV_WINDOWS7 ? false :
           (GetSystemMetrics(QT_SM_DIGITIZER) & (QT_NID_INTEGRATED_TOUCH | QT_NID_EXTERNAL_TOUCH | QT_NID_MULTI_INPUT)) != 0;
}

#if !defined(LANG_SYRIAC)
#    define LANG_SYRIAC 0x5a
#endif

static inline bool useRTL_Extensions(QSysInfo::WinVersion ver)
{
    // This is SDK dependent on CE so out of scope for now
    if (QSysInfo::windowsVersion() & QSysInfo::WV_CE_based)
        return false;
    if ((ver & QSysInfo::WV_NT_based) && (ver >= QSysInfo::WV_VISTA)) {
        // Since the IsValidLanguageGroup/IsValidLocale functions always return true on
        // Vista, check the Keyboard Layouts for enabling RTL.
        if (const UINT nLayouts = GetKeyboardLayoutList(0, 0)) {
            QScopedArrayPointer<HKL> lpList(new HKL[nLayouts]);
            GetKeyboardLayoutList(nLayouts, lpList.data());
            for (UINT i = 0; i < nLayouts; ++i) {
                switch (PRIMARYLANGID((quintptr)lpList[i])) {
                case LANG_ARABIC:
                case LANG_HEBREW:
                case LANG_FARSI:
                case LANG_SYRIAC:
                    return true;
                default:
                    break;
                }
            }
        }
        return false;
    } // NT/Vista
#ifndef Q_OS_WINCE
    // Pre-NT: figure out whether a RTL language is installed
    return IsValidLanguageGroup(LGRPID_ARABIC, LGRPID_INSTALLED)
                            || IsValidLanguageGroup(LGRPID_HEBREW, LGRPID_INSTALLED)
                            || IsValidLocale(MAKELCID(MAKELANGID(LANG_ARABIC, SUBLANG_DEFAULT), SORT_DEFAULT), LCID_INSTALLED)
                            || IsValidLocale(MAKELCID(MAKELANGID(LANG_HEBREW, SUBLANG_DEFAULT), SORT_DEFAULT), LCID_INSTALLED)
                            || IsValidLocale(MAKELCID(MAKELANGID(LANG_SYRIAC, SUBLANG_DEFAULT), SORT_DEFAULT), LCID_INSTALLED)
                            || IsValidLocale(MAKELCID(MAKELANGID(LANG_FARSI, SUBLANG_DEFAULT), SORT_DEFAULT), LCID_INSTALLED);
#else
    return false;
#endif
}

/*!
    \class QWindowsUser32DLL
    \brief Struct that contains dynamically resolved symbols of User32.dll.

    The stub libraries shipped with the MinGW compiler miss some of the
    functions. They need to be retrieved dynamically.

    In addition, touch-related functions are available only from Windows onwards.
    These need to resolved dynamically for Q_CC_MSVC as well.

    \sa QWindowsShell32DLL

    \internal
    \ingroup qt-lighthouse-win
*/

#ifndef Q_OS_WINCE

QWindowsUser32DLL::QWindowsUser32DLL() :
    setLayeredWindowAttributes(0), updateLayeredWindow(0),
    updateLayeredWindowIndirect(0),
    isHungAppWindow(0),
    registerTouchWindow(0), unregisterTouchWindow(0),
    getTouchInputInfo(0), closeTouchInputHandle(0)
{
}

void QWindowsUser32DLL::init()
{
    QSystemLibrary library(QStringLiteral("user32"));
    // MinGW (g++ 3.4.5) accepts only C casts.
    setLayeredWindowAttributes = (SetLayeredWindowAttributes)(library.resolve("SetLayeredWindowAttributes"));
    updateLayeredWindow = (UpdateLayeredWindow)(library.resolve("UpdateLayeredWindow"));
    if (!setLayeredWindowAttributes || !updateLayeredWindow)
        qFatal("This version of Windows is not supported (User32.dll is missing the symbols 'SetLayeredWindowAttributes', 'UpdateLayeredWindow').");

    updateLayeredWindowIndirect = (UpdateLayeredWindowIndirect)(library.resolve("UpdateLayeredWindowIndirect"));
    isHungAppWindow = (IsHungAppWindow)library.resolve("IsHungAppWindow");
}

bool QWindowsUser32DLL::initTouch()
{
    QSystemLibrary library(QStringLiteral("user32"));
    registerTouchWindow = (RegisterTouchWindow)(library.resolve("RegisterTouchWindow"));
    unregisterTouchWindow = (UnregisterTouchWindow)(library.resolve("UnregisterTouchWindow"));
    getTouchInputInfo = (GetTouchInputInfo)(library.resolve("GetTouchInputInfo"));
    closeTouchInputHandle = (CloseTouchInputHandle)(library.resolve("CloseTouchInputHandle"));
    return registerTouchWindow && unregisterTouchWindow && getTouchInputInfo && getTouchInputInfo;
}

/*!
    \class QWindowsShell32DLL
    \brief Struct that contains dynamically resolved symbols of Shell32.dll.

    The stub libraries shipped with the MinGW compiler miss some of the
    functions. They need to be retrieved dynamically.

    \sa QWindowsUser32DLL

    \internal
    \ingroup qt-lighthouse-win
*/

QWindowsShell32DLL::QWindowsShell32DLL()
    : sHCreateItemFromParsingName(0)
    , sHGetStockIconInfo(0)
{
}

void QWindowsShell32DLL::init()
{
    QSystemLibrary library(QStringLiteral("shell32"));
    sHCreateItemFromParsingName = (SHCreateItemFromParsingName)(library.resolve("SHCreateItemFromParsingName"));
    sHGetStockIconInfo = (SHGetStockIconInfo)library.resolve("SHGetStockIconInfo");
}

QWindowsUser32DLL QWindowsContext::user32dll;
QWindowsShell32DLL QWindowsContext::shell32dll;

#endif // !Q_OS_WINCE

QWindowsContext *QWindowsContext::m_instance = 0;

/*!
    \class QWindowsContext
    \brief Singleton container for all relevant information.

    Holds state information formerly stored in \c qapplication_win.cpp.

    \internal
    \ingroup qt-lighthouse-win
*/

typedef QHash<HWND, QWindowsWindow *> HandleBaseWindowHash;

struct QWindowsContextPrivate {

    QWindowsContextPrivate();

    unsigned m_systemInfo;
    QSet<QString> m_registeredWindowClassNames;
    HandleBaseWindowHash m_windows;
    HDC m_displayContext;
    const int m_defaultDPI;
    QWindowsKeyMapper m_keyMapper;
    QWindowsMouseHandler m_mouseHandler;
    QWindowsMimeConverter m_mimeConverter;
    QWindowsScreenManager m_screenManager;
    QSharedPointer<QWindowCreationContext> m_creationContext;
    const HRESULT m_oleInitializeResult;
    const QByteArray m_eventType;
    QWindow *m_lastActiveWindow;
    bool m_asyncExpose;
};

QWindowsContextPrivate::QWindowsContextPrivate() :
    m_systemInfo(0),
    m_displayContext(GetDC(0)),
    m_defaultDPI(GetDeviceCaps(m_displayContext,LOGPIXELSY)),
    m_oleInitializeResult(OleInitialize(NULL)),
    m_eventType(QByteArrayLiteral("windows_generic_MSG")),
    m_lastActiveWindow(0), m_asyncExpose(0)
{
#ifndef Q_OS_WINCE
    QWindowsContext::user32dll.init();
    QWindowsContext::shell32dll.init();
#endif

    const QSysInfo::WinVersion ver = QSysInfo::windowsVersion();
#ifndef Q_OS_WINCE
    if (hasTouchSupport(ver) && QWindowsContext::user32dll.initTouch())
        m_systemInfo |= QWindowsContext::SI_SupportsTouch;
#endif

    if (useRTL_Extensions(ver)) {
        m_systemInfo |= QWindowsContext::SI_RTL_Extensions;
        m_keyMapper.setUseRTLExtensions(true);
    }
}

QWindowsContext::QWindowsContext() :
    d(new QWindowsContextPrivate)
{
#ifdef Q_CC_MSVC
#    pragma warning( disable : 4996 )
#endif
    m_instance = this;
    const QByteArray bv = qgetenv("QT_QPA_VERBOSE");
    if (!bv.isEmpty()) {
        const char *v = bv.data();
        QWindowsContext::verboseIntegration = componentVerbose(v, "integration");
        QWindowsContext::verboseWindows = componentVerbose(v, "windows");
        QWindowsContext::verboseEvents = componentVerbose(v, "events");
        QWindowsContext::verboseBackingStore = componentVerbose(v, "backingstore");
        QWindowsContext::verboseFonts = componentVerbose(v, "fonts");
        QWindowsContext::verboseGL = componentVerbose(v, "gl");
        QWindowsContext::verboseOLE = componentVerbose(v, "ole");
        QWindowsContext::verboseInputMethods = componentVerbose(v, "im");
        QWindowsContext::verboseDialogs = componentVerbose(v, "dialogs");
        QWindowsContext::verboseTheming = componentVerbose(v, "theming");
    }
}

QWindowsContext::~QWindowsContext()
{
    unregisterWindowClasses();
    if (d->m_oleInitializeResult == S_OK || d->m_oleInitializeResult == S_FALSE)
        OleUninitialize();

    d->m_screenManager.clearScreens(); // Order: Potentially calls back to the windows.
    m_instance = 0;
}

QWindowsContext *QWindowsContext::instance()
{
    return m_instance;
}

unsigned QWindowsContext::systemInfo() const
{
    return d->m_systemInfo;
}

bool QWindowsContext::useRTLExtensions() const
{
    return d->m_keyMapper.useRTLExtensions();
}

QList<int> QWindowsContext::possibleKeys(const QKeyEvent *e) const
{
    return d->m_keyMapper.possibleKeys(e);
}

void QWindowsContext::setWindowCreationContext(const QSharedPointer<QWindowCreationContext> &ctx)
{
    d->m_creationContext = ctx;
}

int QWindowsContext::defaultDPI() const
{
    return d->m_defaultDPI;
}

HDC QWindowsContext::displayContext() const
{
    return d->m_displayContext;
}

QWindow *QWindowsContext::keyGrabber() const
{
    return d->m_keyMapper.keyGrabber();
}

void QWindowsContext::setKeyGrabber(QWindow *w)
{
    d->m_keyMapper.setKeyGrabber(w);
}

// Window class registering code (from qapplication_win.cpp)
// If 0 is passed as the widget pointer, register a window class
// for QWidget as default. This is used in QGLTemporaryContext
// during GL initialization, where we don't want to use temporary
// QWidgets or QGLWidgets, neither do we want to have separate code
// to register window classes.

QString QWindowsContext::registerWindowClass(const QWindow *w, bool isGL)
{
    const Qt::WindowFlags flags = w ? w->flags() : (Qt::WindowFlags)0;
    const Qt::WindowFlags type = flags & Qt::WindowType_Mask;

    uint style = 0;
    bool icon = false;
    QString cname = QStringLiteral("Qt5");
    if (w && isGL) {
        cname += QStringLiteral("QGLWindow");
        style = CS_DBLCLKS|CS_OWNDC;
        icon  = true;
    } else if (w && (flags & Qt::MSWindowsOwnDC)) {
        cname += QStringLiteral("QWindowOwnDC");
        style = CS_DBLCLKS|CS_OWNDC;
        icon  = true;
    } else if (w && (type == Qt::Tool || type == Qt::ToolTip)) {
        style = CS_DBLCLKS;
        if (w->inherits("QTipLabel") || w->inherits("QAlphaWidget")) {
            if ((QSysInfo::WindowsVersion >= QSysInfo::WV_XP
                && (QSysInfo::WindowsVersion & QSysInfo::WV_NT_based))) {
                style |= CS_DROPSHADOW;
            }
            cname += QStringLiteral("QToolTip");
        } else {
            cname += QStringLiteral("QTool");
        }
        style |= CS_SAVEBITS;
        icon = false;
    } else if (w && (type == Qt::Popup)) {
        cname += QStringLiteral("QPopup");
        style = CS_DBLCLKS|CS_SAVEBITS;
        if ((QSysInfo::WindowsVersion >= QSysInfo::WV_XP
            && (QSysInfo::WindowsVersion & QSysInfo::WV_NT_based)))
            style |= CS_DROPSHADOW;
        icon = false;
    } else {
        cname += QStringLiteral("QWindow");
        style = CS_DBLCLKS;
        icon  = true;
    }

    HBRUSH brush = 0;
    if (w && !isGL)
        brush = GetSysColorBrush(COLOR_WINDOW);
    return registerWindowClass(cname, qWindowsWndProc, style, brush, icon);
}

QString QWindowsContext::registerWindowClass(QString cname,
                                             WNDPROC proc,
                                             unsigned style,
                                             HBRUSH brush,
                                             bool icon)
{
    // since multiple Qt versions can be used in one process
    // each one has to have window class names with a unique name
    // The first instance gets the unmodified name; if the class
    // has already been registered by another instance of Qt then
    // add an instance-specific ID, the address of the window proc.
    static int classExists = -1;

    const HINSTANCE appInstance = (HINSTANCE)GetModuleHandle(0);
    if (classExists == -1) {
        WNDCLASS wcinfo;
        classExists = GetClassInfo(appInstance, (wchar_t*)cname.utf16(), &wcinfo);
        classExists = classExists && wcinfo.lpfnWndProc != proc;
    }

    if (classExists)
        cname += QString::number((quintptr)proc);

    if (d->m_registeredWindowClassNames.contains(cname))        // already registered in our list
        return cname;

#ifndef Q_OS_WINCE
    WNDCLASSEX wc;
    wc.cbSize       = sizeof(WNDCLASSEX);
#else
    WNDCLASS wc;
#endif
    wc.style        = style;
    wc.lpfnWndProc  = proc;
    wc.cbClsExtra   = 0;
    wc.cbWndExtra   = 0;
    wc.hInstance    = appInstance;
    wc.hCursor      = 0;
#ifndef Q_OS_WINCE
    wc.hbrBackground = brush;
    if (icon) {
        wc.hIcon = (HICON)LoadImage(appInstance, L"IDI_ICON1", IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
        if (wc.hIcon) {
            int sw = GetSystemMetrics(SM_CXSMICON);
            int sh = GetSystemMetrics(SM_CYSMICON);
            wc.hIconSm = (HICON)LoadImage(appInstance, L"IDI_ICON1", IMAGE_ICON, sw, sh, 0);
        } else {
            wc.hIcon = (HICON)LoadImage(0, IDI_APPLICATION, IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
            wc.hIconSm = 0;
        }
    } else {
        wc.hIcon    = 0;
        wc.hIconSm  = 0;
    }
#else
    if (icon) {
        wc.hIcon = (HICON)LoadImage(appInstance, L"IDI_ICON1", IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    } else {
        wc.hIcon    = 0;
    }
#endif

    wc.lpszMenuName  = 0;
    wc.lpszClassName = (wchar_t*)cname.utf16();
#ifndef Q_OS_WINCE
    ATOM atom = RegisterClassEx(&wc);
#else
    ATOM atom = RegisterClass(&wc);
#endif

    if (!atom)
        qErrnoWarning("QApplication::regClass: Registering window class '%s' failed.",
                      qPrintable(cname));

    d->m_registeredWindowClassNames.insert(cname);
    if (QWindowsContext::verboseIntegration || QWindowsContext::verboseWindows)
        qDebug().nospace() << __FUNCTION__ << ' ' << cname
                 << " style=0x" << QString::number(style, 16)
                 << " brush=" << brush << " icon=" << icon << " atom=" << atom;
    return cname;
}

void QWindowsContext::unregisterWindowClasses()
{
    const HINSTANCE appInstance = (HINSTANCE)GetModuleHandle(0);

    foreach (const QString &name,  d->m_registeredWindowClassNames) {
        if (QWindowsContext::verboseIntegration)
            qDebug() << __FUNCTION__ << name;
        UnregisterClass((wchar_t*)name.utf16(), appInstance);
    }
    d->m_registeredWindowClassNames.clear();
}

int QWindowsContext::screenDepth() const
{
    return GetDeviceCaps(d->m_displayContext, BITSPIXEL);
}

QString QWindowsContext::windowsErrorMessage(unsigned long errorCode)
{
    QString rc = QString::fromLatin1("#%1: ").arg(errorCode);
    ushort *lpMsgBuf;

    const int len = FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, errorCode, 0, (LPTSTR)&lpMsgBuf, 0, NULL);
    if (len) {
        rc = QString::fromUtf16(lpMsgBuf, len);
        LocalFree(lpMsgBuf);
    } else {
        rc += QString::fromLatin1("<unknown error>");
    }
    return rc;
}

void QWindowsContext::addWindow(HWND hwnd, QWindowsWindow *w)
{
    d->m_windows.insert(hwnd, w);
}

void QWindowsContext::removeWindow(HWND hwnd)
{
    const HandleBaseWindowHash::iterator it = d->m_windows.find(hwnd);
    if (it != d->m_windows.end()) {
        if (d->m_keyMapper.keyGrabber() == it.value()->window())
            d->m_keyMapper.setKeyGrabber(0);
        d->m_windows.erase(it);
    }
}

QWindowsWindow *QWindowsContext::findPlatformWindow(HWND hwnd) const
{
    return d->m_windows.value(hwnd);
}

QWindowsWindow *QWindowsContext::findClosestPlatformWindow(HWND hwnd) const
{
    QWindowsWindow *window = d->m_windows.value(hwnd);

    // Requested hwnd may also be a child of a platform window in case of embedded native windows.
    // Find the closest parent that has a platform window.
    if (!window) {
        for (HWND w = hwnd; w; w = GetParent(w)) {
            window = d->m_windows.value(w);
            if (window)
                break;
        }
    }

    return window;
}

QWindow *QWindowsContext::findWindow(HWND hwnd) const
{
    if (const QWindowsWindow *bw = findPlatformWindow(hwnd))
            return bw->window();
    return 0;
}

QWindow *QWindowsContext::windowUnderMouse() const
{
    return d->m_mouseHandler.windowUnderMouse();
}

/*!
    \brief Find a child window at a screen point.

    Deep search for a QWindow at global point, skipping non-owned
    windows (accessibility?). Implemented using ChildWindowFromPointEx()
    instead of (historically used) WindowFromPoint() to get a well-defined
    behaviour for hidden/transparent windows.

    \a cwex_flags are flags of ChildWindowFromPointEx().
    \a parent is the parent window, pass GetDesktopWindow() for top levels.
*/

QWindowsWindow *QWindowsContext::findPlatformWindowAt(HWND parent,
                                                          const QPoint &screenPointIn,
                                                          unsigned cwex_flags) const
{
    QWindowsWindow *result = 0;
    const POINT screenPoint = { screenPointIn.x(), screenPointIn.y() };
    while (true) {
        POINT point = screenPoint;
        ScreenToClient(parent, &point);
        // Returns parent if inside & none matched.
        const HWND child = ChildWindowFromPointEx(parent, point, cwex_flags);
        if (child && child != parent) {
            if (QWindowsWindow *window = findPlatformWindow(child))
                result = window;
            parent = child;
        } else {
            break;
        }
    }
    return result;
}

QWindowsMimeConverter &QWindowsContext::mimeConverter() const
{
    return d->m_mimeConverter;
}

QWindowsScreenManager &QWindowsContext::screenManager()
{
    return d->m_screenManager;
}

/*!
    \brief Convenience to create a non-visible, message-only dummy
    window for example used as clipboard watcher or for GL.
*/

HWND QWindowsContext::createDummyWindow(const QString &classNameIn,
                                        const wchar_t *windowName,
                                        WNDPROC wndProc, DWORD style)
{
    if (!wndProc)
        wndProc = DefWindowProc;
    QString className = registerWindowClass(classNameIn, wndProc);
    return CreateWindowEx(0, (wchar_t*)className.utf16(),
                          windowName, style,
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          HWND_MESSAGE, NULL, (HINSTANCE)GetModuleHandle(0), NULL);
}

/*!
    \brief Common COM error strings.
*/

QByteArray QWindowsContext::comErrorString(HRESULT hr)
{
    switch (hr) {
    case S_OK:
        return QByteArrayLiteral("S_OK");
    case S_FALSE:
        return QByteArrayLiteral("S_FALSE");
    case E_UNEXPECTED:
        return QByteArrayLiteral("E_UNEXPECTED");
    case CO_E_ALREADYINITIALIZED:
        return QByteArrayLiteral("CO_E_ALREADYINITIALIZED");
    case CO_E_NOTINITIALIZED:
        return QByteArrayLiteral("CO_E_NOTINITIALIZED");
    case RPC_E_CHANGED_MODE:
        return QByteArrayLiteral("RPC_E_CHANGED_MODE");
    case OLE_E_WRONGCOMPOBJ:
        return QByteArrayLiteral("OLE_E_WRONGCOMPOBJ");
    case CO_E_NOT_SUPPORTED:
        return QByteArrayLiteral("CO_E_NOT_SUPPORTED");
    case E_NOTIMPL:
        return QByteArrayLiteral("E_NOTIMPL");
    case E_INVALIDARG:
        return QByteArrayLiteral("E_INVALIDARG");
    case E_NOINTERFACE:
        return QByteArrayLiteral("E_NOINTERFACE");
    case E_POINTER:
        return QByteArrayLiteral("E_POINTER");
    case E_HANDLE:
        return QByteArrayLiteral("E_HANDLE");
    case E_ABORT:
        return QByteArrayLiteral("E_ABORT");
    case E_FAIL:
        return QByteArrayLiteral("E_FAIL");
    case RPC_E_WRONG_THREAD:
        return QByteArrayLiteral("RPC_E_WRONG_THREAD");
    case RPC_E_THREAD_NOT_INIT:
        return QByteArrayLiteral("RPC_E_THREAD_NOT_INIT");
    default:
        break;
    }
    return "Unknown error 0x" + QByteArray::number(quint64(hr), 16);
}

/*!
     \brief Main windows procedure registered for windows.

     \sa QWindowsGuiEventDispatcher
*/

bool QWindowsContext::windowsProc(HWND hwnd, UINT message,
                                  QtWindows::WindowsEventType et,
                                  WPARAM wParam, LPARAM lParam, LRESULT *result)
{
    *result = 0;

    MSG msg;
    msg.hwnd = hwnd;         // re-create MSG structure
    msg.message = message;   // time and pt fields ignored
    msg.wParam = wParam;
    msg.lParam = lParam;
    msg.pt.x = GET_X_LPARAM(lParam);
    msg.pt.y = GET_Y_LPARAM(lParam);

    // Run the native event filters.
    long filterResult = 0;
    QAbstractEventDispatcher* dispatcher = QAbstractEventDispatcher::instance();
    if (dispatcher && dispatcher->filterNativeEvent(d->m_eventType, &msg, &filterResult)) {
        *result = LRESULT(filterResult);
        return true;
    }

    QWindowsWindow *platformWindow = findPlatformWindow(hwnd);
    if (platformWindow) {
        filterResult = 0;
        if (QWindowSystemInterface::handleNativeEvent(platformWindow->window(), d->m_eventType, &msg, &filterResult)) {
            *result = LRESULT(filterResult);
            return true;
        }
    }

    switch (et) {
    case QtWindows::InputMethodStartCompositionEvent:
        return QWindowsInputContext::instance()->startComposition(hwnd);
    case QtWindows::InputMethodCompositionEvent:
        return QWindowsInputContext::instance()->composition(hwnd, lParam);
    case QtWindows::InputMethodEndCompositionEvent:
        return QWindowsInputContext::instance()->endComposition(hwnd);
    case QtWindows::InputMethodRequest:
        return QWindowsInputContext::instance()->handleIME_Request(wParam, lParam, result);
    case QtWindows::InputMethodOpenCandidateWindowEvent:
    case QtWindows::InputMethodCloseCandidateWindowEvent:
        // TODO: Release/regrab mouse if a popup has mouse grab.
        return false;
    case QtWindows::ClipboardEvent:
    case QtWindows::DestroyEvent:

    case QtWindows::UnknownEvent:
        return false;
    case QtWindows::AccessibleObjectFromWindowRequest:
#ifndef QT_NO_ACCESSIBILITY
        return QWindowsAccessibility::handleAccessibleObjectFromWindowRequest(hwnd, wParam, lParam, result);
#else
        return false;
#endif
    case QtWindows::DisplayChangedEvent:
        return d->m_screenManager.handleDisplayChange(wParam, lParam);
    case QtWindows::SettingChangedEvent:
        return d->m_screenManager.handleScreenChanges();
    default:
        break;
    }

    // Before CreateWindowEx() returns, some events are sent,
    // for example WM_GETMINMAXINFO asking for size constraints for top levels.
    // Pass on to current creation context
    if (!platformWindow && !d->m_creationContext.isNull()) {
        switch (et) {
#ifndef Q_OS_WINCE // maybe available on some SDKs revisit WM_GETMINMAXINFO
        case QtWindows::QuerySizeHints:
            d->m_creationContext->applyToMinMaxInfo(reinterpret_cast<MINMAXINFO *>(lParam));
            return true;
#endif
        case QtWindows::ResizeEvent:
            d->m_creationContext->obtainedGeometry.setSize(QSize(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
            return true;
        case QtWindows::MoveEvent:
            d->m_creationContext->obtainedGeometry.moveTo(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return true;
        case QtWindows::CalculateSize:
            return QWindowsGeometryHint::handleCalculateSize(d->m_creationContext->customMargins, msg, result);
        default:
            break;
        }
    }
    if (platformWindow) {
        // Suppress events sent during DestroyWindow() for native children.
        if (platformWindow->testFlag(QWindowsWindow::WithinDestroy))
            return false;
        if (QWindowsContext::verboseEvents > 1)
            qDebug().nospace() << "Event window: " << platformWindow->window();
    } else {
        qWarning("%s: No Qt Window found for event 0x%x (%s), hwnd=0x%p.",
                 __FUNCTION__, message,
                 QWindowsGuiEventDispatcher::windowsMessageName(message), hwnd);
        return false;
    }

    switch (et) {
    case QtWindows::KeyDownEvent:
    case QtWindows::KeyEvent:
    case QtWindows::InputMethodKeyEvent:
    case QtWindows::InputMethodKeyDownEvent:
        return d->m_keyMapper.translateKeyEvent(platformWindow->window(), hwnd, msg, result);
    case QtWindows::MoveEvent:
        platformWindow->handleMoved();
        return true;
    case QtWindows::ResizeEvent:
        platformWindow->handleResized((int)wParam);
        return true;
#ifndef Q_OS_WINCE // maybe available on some SDKs revisit WM_GETMINMAXINFO
    case QtWindows::QuerySizeHints:
        platformWindow->getSizeHints(reinterpret_cast<MINMAXINFO *>(lParam));
        return true;// maybe available on some SDKs revisit WM_NCCALCSIZE
    case QtWindows::CalculateSize:
        return QWindowsGeometryHint::handleCalculateSize(platformWindow->customMargins(), msg, result);
#endif
    case QtWindows::ExposeEvent:
        return platformWindow->handleWmPaint(hwnd, message, wParam, lParam);
    case QtWindows::NonClientMouseEvent:
        if (platformWindow->frameStrutEventsEnabled())
            return d->m_mouseHandler.translateMouseEvent(platformWindow->window(), hwnd, et, msg, result);
        break;
/* the mouse tracking on windows already handles the reset of the cursor
 * and does not like somebody else handling it.
 * on WINCE its necessary to handle this event to get the correct cursor
 */
#ifdef Q_OS_WINCE
    case QtWindows::CursorEvent:
        {
            QWindowsWindow::baseWindowOf(platformWindow->window())->applyCursor();
            return true;
        }
#endif
    case QtWindows::MouseWheelEvent:
    case QtWindows::MouseEvent:
    case QtWindows::LeaveEvent:
        return d->m_mouseHandler.translateMouseEvent(platformWindow->window(), hwnd, et, msg, result);
    case QtWindows::TouchEvent:
        return d->m_mouseHandler.translateTouchEvent(platformWindow->window(), hwnd, et, msg, result);
    case QtWindows::FocusInEvent: // see QWindowsWindow::requestActivateWindow().
    case QtWindows::FocusOutEvent:
        handleFocusEvent(et, platformWindow);
        return true;
    case QtWindows::ShowEvent:
        platformWindow->handleShown();
        return true;
    case QtWindows::HideEvent:
        platformWindow->handleHidden();
        return true;
    case QtWindows::CloseEvent:
        QWindowSystemInterface::handleCloseEvent(platformWindow->window());
        return true;
    case QtWindows::ThemeChanged: // ### fixme: Compress these events?
        if (QWindowsTheme *theme = QWindowsTheme::instance())
            theme->windowsThemeChanged(platformWindow->window());
        return true;
#ifndef Q_OS_WINCE
    case QtWindows::ActivateWindowEvent:
        if (platformWindow->testFlag(QWindowsWindow::BlockedByModal))
            if (const QWindow *modalWindow = QGuiApplication::modalWindow())
                QWindowsWindow::baseWindowOf(modalWindow)->alertWindow();
        break;
#endif
#ifndef QT_NO_CONTEXTMENU
    case QtWindows::ContextMenu:
        handleContextMenuEvent(platformWindow->window(), msg);
        return true;
#endif
    default:
        break;
    }
    return false;
}

/* Compress activation events. If the next focus window is already known
 * at the time the current one receives focus-out, pass that to
 * QWindowSystemInterface instead of sending 0 and ignore its consecutive
 * focus-in event.
 * This helps applications that do handling in focus-out events. */
void QWindowsContext::handleFocusEvent(QtWindows::WindowsEventType et,
                                       QWindowsWindow *platformWindow)
{
    QWindow *nextActiveWindow = 0;
    if (et == QtWindows::FocusInEvent) {
        nextActiveWindow = platformWindow->window();
    } else {
        // Focus out: Is the next window known and different
        // from the receiving the focus out.
        if (const HWND nextActiveHwnd = GetFocus())
            if (QWindowsWindow *nextActivePlatformWindow = findClosestPlatformWindow(nextActiveHwnd))
                if (nextActivePlatformWindow != platformWindow)
                    nextActiveWindow = nextActivePlatformWindow->window();
    }
    if (nextActiveWindow != d->m_lastActiveWindow) {
         d->m_lastActiveWindow = nextActiveWindow;
         QWindowSystemInterface::handleWindowActivated(nextActiveWindow);
    }
}

#ifndef QT_NO_CONTEXTMENU
void QWindowsContext::handleContextMenuEvent(QWindow *window, const MSG &msg)
{
    bool mouseTriggered = false;
    QPoint globalPos;
    QPoint pos;
    if (msg.lParam != (int)0xffffffff) {
        mouseTriggered = true;
        globalPos.setX(msg.pt.x);
        globalPos.setY(msg.pt.y);
        pos = QWindowsGeometryHint::mapFromGlobal(msg.hwnd, globalPos);
    }

    QWindowSystemInterface::handleContextMenuEvent(window, mouseTriggered, pos, globalPos,
                                                   QWindowsKeyMapper::queryKeyboardModifiers());
}
#endif

bool QWindowsContext::asyncExpose() const
{
    return d->m_asyncExpose;
}

void QWindowsContext::setAsyncExpose(bool value)
{
    d->m_asyncExpose = value;
}

/*!
    \brief Windows functions for actual windows.

    There is another one for timers, sockets, etc in
    QEventDispatcherWin32.

    \ingroup qt-lighthouse-win
*/

extern "C" LRESULT QT_WIN_CALLBACK qWindowsWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result;
    const QtWindows::WindowsEventType et = windowsEventType(message, wParam);
    const bool handled = QWindowsContext::instance()->windowsProc(hwnd, message, et, wParam, lParam, &result);
    if (QWindowsContext::verboseEvents > 1)
        if (const char *eventName = QWindowsGuiEventDispatcher::windowsMessageName(message))
            qDebug("EVENT: hwd=%p %s msg=0x%x et=0x%x wp=%d at %d,%d handled=%d",
                   hwnd, eventName, message, et, int(wParam),
                   GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), handled);
    if (!handled)
        result = DefWindowProc(hwnd, message, wParam, lParam);
    return result;
}

QT_END_NAMESPACE
