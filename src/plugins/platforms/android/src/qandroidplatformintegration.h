/****************************************************************************
**
** Copyright (C) 2012 BogDan Vatra <bogdan@kde.org>
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

#ifndef QANDROIDPLATFORMINTERATION_H
#define QANDROIDPLATFORMINTERATION_H

#include <qpa/qplatformintegration.h>
#include <qpa/qplatformmenu.h>
#include <qpa/qplatformnativeinterface.h>
#include <QtWidgets/QAction>

#include <jni.h>
#include "qandroidinputcontext.h"

#ifndef ANDROID_PLUGIN_OPENGL
#  include "qandroidplatformscreen.h"
#else
#  include "qeglfsintegration.h"
#endif

QT_BEGIN_NAMESPACE

class QDesktopWidget;
class QAndroidPlatformServices;

#ifdef ANDROID_PLUGIN_OPENGL
class QAndroidOpenGLPlatformWindow;
#endif

class QAndroidPlatformNativeInterface: public QPlatformNativeInterface
{
public:
    void *nativeResourceForIntegration(const QByteArray &resource);
};

class QAndroidPlatformIntegration
#ifndef ANDROID_PLUGIN_OPENGL
    : public QPlatformIntegration
#else
    : public QEglFSIntegration
#endif
{
    friend class QAndroidPlatformScreen;

public:
    QAndroidPlatformIntegration(const QStringList &paramList);
    ~QAndroidPlatformIntegration();

    bool hasCapability(QPlatformIntegration::Capability cap) const;

#ifndef ANDROID_PLUGIN_OPENGL
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const;
    QPlatformWindow *createPlatformWindow(QWindow *window) const;
    QAbstractEventDispatcher *guiThreadEventDispatcher() const;
    QAndroidPlatformScreen *screen() { return m_primaryScreen; }
#else
    QPlatformWindow *createPlatformWindow(QWindow *window) const;
    void invalidateNativeSurface();
    void surfaceChanged();
    QAndroidOpenGLPlatformWindow *primaryWindow() const { return m_primaryWindow; }
    QPlatformOpenGLContext *createPlatformOpenGLContext(QOpenGLContext *context) const;
#endif

    virtual void setDesktopSize(int width, int height);
    virtual void setDisplayMetrics(int width, int height);
    bool isVirtualDesktop() { return true; }

    QPlatformFontDatabase *fontDatabase() const;

#ifndef QT_NO_CLIPBOARD
    QPlatformClipboard *clipboard() const;
#endif

    QPlatformInputContext *inputContext() const;
    QPlatformNativeInterface *nativeInterface() const;
    QPlatformServices *services() const;

    QStringList themeNames() const;
    QPlatformTheme *createPlatformTheme(const QString &name) const;

    void pauseApp();
    void resumeApp();
    static void setDefaultDisplayMetrics(int gw, int gh, int sw, int sh);
    static void setDefaultDesktopSize(int gw, int gh);

    static QSize defaultDesktopSize()
    {
        return QSize(m_defaultGeometryWidth, m_defaultGeometryHeight);
    }

private:

    friend class QEglFSAndroidHooks;
#ifndef ANDROID_PLUGIN_OPENGL
    QAbstractEventDispatcher *m_eventDispatcher;
    QAndroidPlatformScreen *m_primaryScreen;
#else
    mutable QAndroidOpenGLPlatformWindow *m_primaryWindow;
#endif

    QThread *m_mainThread;

    static int m_defaultGeometryWidth;
    static int m_defaultGeometryHeight;
    static int m_defaultPhysicalSizeWidth;
    static int m_defaultPhysicalSizeHeight;

    QPlatformFontDatabase *m_androidFDB;
    QImage *m_FbScreenImage;
    QPainter *m_compositePainter;
    QAndroidPlatformNativeInterface *m_androidPlatformNativeInterface;
    QAndroidPlatformServices *m_androidPlatformServices;
    QPlatformClipboard *m_androidPlatformClipboard;

    mutable QAndroidInputContext m_platformInputContext;
};

QT_END_NAMESPACE

#endif
