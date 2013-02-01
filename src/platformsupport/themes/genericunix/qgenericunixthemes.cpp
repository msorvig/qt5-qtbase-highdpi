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

#include "qgenericunixthemes_p.h"
#include "../../services/genericunix/qgenericunixservices_p.h"

#include <QtGui/QPalette>
#include <QtGui/QFont>
#include <QtGui/QGuiApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QFile>
#include <QtCore/QDebug>
#include <QtCore/QSettings>
#include <QtCore/QVariant>
#include <QtCore/QStringList>
#include <private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>
#include <qpa/qplatformservices.h>

QT_BEGIN_NAMESPACE

ResourceHelper::ResourceHelper()
{
    qFill(palettes, palettes + QPlatformTheme::NPalettes, static_cast<QPalette *>(0));
    qFill(fonts, fonts + QPlatformTheme::NFonts, static_cast<QFont *>(0));
}

void ResourceHelper::clear()
{
    qDeleteAll(palettes, palettes + QPlatformTheme::NPalettes);
    qDeleteAll(fonts, fonts + QPlatformTheme::NFonts);
    qFill(palettes, palettes + QPlatformTheme::NPalettes, static_cast<QPalette *>(0));
    qFill(fonts, fonts + QPlatformTheme::NFonts, static_cast<QFont *>(0));
}

/*!
    \class QGenericX11ThemeQKdeTheme
    \brief QGenericX11Theme is a generic theme implementation for X11.
    \since 5.0
    \internal
    \ingroup qpa
*/

const char *QGenericUnixTheme::name = "generic";

// Default system font, corresponding to the value returned by 4.8 for
// XRender/FontConfig which we can now assume as default.
static const char defaultSystemFontNameC[] = "Sans Serif";
enum { defaultSystemFontSize = 9 };

QGenericUnixTheme::QGenericUnixTheme()
    : m_systemFont(QLatin1String(defaultSystemFontNameC), defaultSystemFontSize)
{
}

const QFont *QGenericUnixTheme::font(Font type) const
{
    if (type == QPlatformTheme::SystemFont)
        return &m_systemFont;
    return 0;
}

// Helper to return the icon theme paths from XDG.
QStringList QGenericUnixTheme::xdgIconThemePaths()
{
    QStringList paths;
    // Add home directory first in search path
    const QFileInfo homeIconDir(QDir::homePath() + QStringLiteral("/.icons"));
    if (homeIconDir.isDir())
        paths.prepend(homeIconDir.absoluteFilePath());

    QString xdgDirString = QFile::decodeName(qgetenv("XDG_DATA_DIRS"));
    if (xdgDirString.isEmpty())
        xdgDirString = QLatin1String("/usr/local/share/:/usr/share/");
    foreach (const QString &xdgDir, xdgDirString.split(QLatin1Char(':'))) {
        const QFileInfo xdgIconsDir(xdgDir + QStringLiteral("/icons"));
        if (xdgIconsDir.isDir())
            paths.append(xdgIconsDir.absoluteFilePath());
    }
    return paths;
}

QVariant QGenericUnixTheme::themeHint(ThemeHint hint) const
{
    switch (hint) {
    case QPlatformTheme::SystemIconFallbackThemeName:
        return QVariant(QString(QStringLiteral("hicolor")));
    case QPlatformTheme::IconThemeSearchPaths:
        return xdgIconThemePaths();
    case QPlatformTheme::DialogButtonBoxButtonsHaveIcons:
        return QVariant(true);
    case QPlatformTheme::StyleNames: {
        QStringList styleNames;
        styleNames << QStringLiteral("Fusion") << QStringLiteral("Windows");
        return QVariant(styleNames);
    }
    case QPlatformTheme::KeyboardScheme:
        return QVariant(int(X11KeyboardScheme));
    default:
        break;
    }
    return QPlatformTheme::themeHint(hint);
}

#ifndef QT_NO_SETTINGS

// Reads the color from the KDE configuration, and store it in the
// palette with the given color role if found.
static inline bool kdeColor(QPalette *pal, QPalette::ColorRole role,
                            const QSettings &kdeSettings, const QString &key)
{
    const QVariant value = kdeSettings.value(key);
    if (!value.isValid())
        return false;
    const QStringList values = value.toStringList();
    if (values.size() != 3)
        return false;
    pal->setBrush(role, QColor(values.at(0).toInt(), values.at(1).toInt(), values.at(2).toInt()));
    return true;
}

static inline void readKdeSystemPalette(const QSettings &kdeSettings, QPalette *pal)
{
    kdeColor(pal, QPalette::Button, kdeSettings, QStringLiteral("Colors:Button/BackgroundNormal"));
    kdeColor(pal, QPalette::Window, kdeSettings, QStringLiteral("Colors:Window/BackgroundNormal"));
    kdeColor(pal, QPalette::Text, kdeSettings, QStringLiteral("Colors:View/ForegroundNormal"));
    kdeColor(pal, QPalette::WindowText, kdeSettings, QStringLiteral("Colors:Window/ForegroundNormal"));
    kdeColor(pal, QPalette::Base, kdeSettings, QStringLiteral("Colors:View/BackgroundNormal"));
    kdeColor(pal, QPalette::Highlight, kdeSettings, QStringLiteral("Colors:Selection/BackgroundNormal"));
    kdeColor(pal, QPalette::HighlightedText, kdeSettings, QStringLiteral("Colors:Selection/ForegroundNormal"));
    kdeColor(pal, QPalette::AlternateBase, kdeSettings, QStringLiteral("Colors:View/BackgroundAlternate"));
    kdeColor(pal, QPalette::ButtonText, kdeSettings, QStringLiteral("Colors:Button/ForegroundNormal"));
    kdeColor(pal, QPalette::Link, kdeSettings, QStringLiteral("Colors:View/ForegroundLink"));
    kdeColor(pal, QPalette::LinkVisited, kdeSettings, QStringLiteral("Colors:View/ForegroundVisited"));
}

/*!
    \class QKdeTheme
    \brief QKdeTheme is a theme implementation for the KDE desktop (version 4 or higher).
    \since 5.0
    \internal
    \ingroup qpa
*/

const char *QKdeTheme::name = "kde";

QKdeTheme::QKdeTheme(const QString &kdeHome, int kdeVersion) :
    m_kdeHome(kdeHome), m_kdeVersion(kdeVersion),
    m_toolButtonStyle(Qt::ToolButtonTextBesideIcon), m_toolBarIconSize(0)
{
    refresh();
}

static inline QFont *readKdeFontSetting(const QSettings &settings, const QString &key)
{
    const QVariant fontValue = settings.value(key);
    if (fontValue.isValid()) {
        // Read font value: Might be a QStringList as KDE stores fonts without quotes.
        // Also retrieve the family for the constructor since we cannot use the
        // default constructor of QFont, which accesses QGuiApplication::systemFont()
        // causing recursion.
        QString fontDescription;
        QString fontFamily;
        if (fontValue.type() == QVariant::StringList) {
            const QStringList list = fontValue.toStringList();
            if (!list.isEmpty()) {
                fontFamily = list.first();
                fontDescription = list.join(QLatin1Char(','));
            }
        } else {
            fontDescription = fontFamily = fontValue.toString();
        }
        if (!fontDescription.isEmpty()) {
            QFont font(fontFamily);
            if (font.fromString(fontDescription))
                return new QFont(font);
        }
    }
    return 0;
}

void QKdeTheme::refresh()
{
    m_resources.clear();

    m_toolButtonStyle = Qt::ToolButtonTextBesideIcon;
    m_toolBarIconSize = 0;
    m_styleNames.clear();
    m_styleNames << QStringLiteral("Oxygen") << QStringLiteral("fusion") << QStringLiteral("windows");
    m_iconFallbackThemeName = m_iconThemeName = QStringLiteral("oxygen");

    // Read settings file.
    const QString settingsFile = globalSettingsFile();
    if (!QFileInfo(settingsFile).isReadable())
        return;

    const QSettings kdeSettings(settingsFile, QSettings::IniFormat);

    QPalette systemPalette = QPalette();
    readKdeSystemPalette(kdeSettings, &systemPalette);
    m_resources.palettes[SystemPalette] = new QPalette(systemPalette);
    //## TODO tooltip color

    const QVariant styleValue = kdeSettings.value(QStringLiteral("widgetStyle"));
    if (styleValue.isValid()) {
        const QString style = styleValue.toString();
        if (style != m_styleNames.front())
            m_styleNames.push_front(style);
    }

    const QVariant themeValue = kdeSettings.value(QStringLiteral("Icons/Theme"));
    if (themeValue.isValid())
        m_iconThemeName = themeValue.toString();

    const QVariant toolBarIconSizeValue = kdeSettings.value(QStringLiteral("ToolbarIcons/Size"));
    if (toolBarIconSizeValue.isValid())
        m_toolBarIconSize = toolBarIconSizeValue.toInt();

    const QVariant toolbarStyleValue = kdeSettings.value(QStringLiteral("ToolButtonStyle"));
    if (toolbarStyleValue.isValid()) {
        const QString toolBarStyle = toolbarStyleValue.toString();
        if (toolBarStyle == QStringLiteral("TextBesideIcon"))
            m_toolButtonStyle =  Qt::ToolButtonTextBesideIcon;
        else if (toolBarStyle == QStringLiteral("TextOnly"))
            m_toolButtonStyle = Qt::ToolButtonTextOnly;
        else if (toolBarStyle == QStringLiteral("TextUnderIcon"))
            m_toolButtonStyle = Qt::ToolButtonTextUnderIcon;
    }

    // Read system font, ignore 'fixed' 'smallestReadableFont'
    if (QFont *systemFont = readKdeFontSetting(kdeSettings, QStringLiteral("font"))) {
        m_resources.fonts[SystemFont] = systemFont;
    } else {
        m_resources.fonts[SystemFont] = new QFont(QLatin1String(defaultSystemFontNameC), defaultSystemFontSize);
    }
}

QString QKdeTheme::globalSettingsFile() const
{
    return m_kdeHome + QStringLiteral("/share/config/kdeglobals");
}

static QStringList kdeIconThemeSearchPaths(const QString &kdeHome)
{
    QStringList candidates = QStringList(kdeHome);
    const QString kdeDirs = QFile::decodeName(qgetenv("KDEDIRS"));
    if (!kdeDirs.isEmpty())
        candidates.append(kdeDirs.split(QLatin1Char(':')));

    QStringList paths = QGenericUnixTheme::xdgIconThemePaths();
    const QString iconPath = QStringLiteral("/share/icons");
    foreach (const QString &candidate, candidates) {
        const QFileInfo fi(candidate + iconPath);
        if (fi.isDir())
            paths.append(fi.absoluteFilePath());
    }
    return paths;
}

QVariant QKdeTheme::themeHint(QPlatformTheme::ThemeHint hint) const
{
    switch (hint) {
    case QPlatformTheme::UseFullScreenForPopupMenu:
        return QVariant(true);
    case QPlatformTheme::DialogButtonBoxButtonsHaveIcons:
        return QVariant(true);
    case QPlatformTheme::DialogButtonBoxLayout:
        return QVariant(2); // QDialogButtonBox::KdeLayout
    case QPlatformTheme::ToolButtonStyle:
        return QVariant(m_toolButtonStyle);
    case QPlatformTheme::ToolBarIconSize:
        return QVariant(m_toolBarIconSize);
    case QPlatformTheme::SystemIconThemeName:
        return QVariant(m_iconThemeName);
    case QPlatformTheme::SystemIconFallbackThemeName:
        return QVariant(m_iconFallbackThemeName);
    case QPlatformTheme::IconThemeSearchPaths:
        return QVariant(kdeIconThemeSearchPaths(m_kdeHome));
    case QPlatformTheme::StyleNames:
        return QVariant(m_styleNames);
    case QPlatformTheme::KeyboardScheme:
        return QVariant(int(KdeKeyboardScheme));
    default:
        break;
    }
    return QPlatformTheme::themeHint(hint);
}

QPlatformTheme *QKdeTheme::createKdeTheme()
{
    // Check for version >= 4 and determine home folder from environment,
    // defaulting to ~/.kde<version>, ~/.kde
    const QByteArray kdeVersionBA = qgetenv("KDE_SESSION_VERSION");
    const int kdeVersion = kdeVersionBA.toInt();
    if (kdeVersion < 4)
        return 0;
    const QString kdeHomePathVar = QString::fromLocal8Bit(qgetenv("KDEHOME"));
    if (!kdeHomePathVar.isEmpty())
        return new QKdeTheme(kdeHomePathVar, kdeVersion);

     const QString kdeVersionHomePath = QDir::homePath() + QStringLiteral("/.kde") + QLatin1String(kdeVersionBA);
     if (QFileInfo(kdeVersionHomePath).isDir())
         return new QKdeTheme(kdeVersionHomePath, kdeVersion);

     const QString kdeHomePath = QDir::homePath() + QStringLiteral("/.kde");
     if (QFileInfo(kdeHomePath).isDir())
         return new QKdeTheme(kdeHomePath, kdeVersion);

     qWarning("%s: Unable to determine KDEHOME", Q_FUNC_INFO);
     return 0;
}

#endif // QT_NO_SETTINGS

/*!
    \class QGnomeTheme
    \brief QGnomeTheme is a theme implementation for the Gnome desktop.
    \since 5.0
    \internal
    \ingroup qpa
*/

const char *QGnomeTheme::name = "gnome";

QGnomeTheme::QGnomeTheme()
   : m_systemFont(QLatin1String(defaultSystemFontNameC), defaultSystemFontSize)
{
}

QVariant QGnomeTheme::themeHint(QPlatformTheme::ThemeHint hint) const
{
    switch (hint) {
    case QPlatformTheme::DialogButtonBoxButtonsHaveIcons:
        return QVariant(true);
    case QPlatformTheme::DialogButtonBoxLayout:
        return QVariant(3); // QDialogButtonBox::GnomeLayout
    case QPlatformTheme::SystemIconThemeName:
    case QPlatformTheme::SystemIconFallbackThemeName:
        return QVariant(QString(QStringLiteral("gnome")));
    case QPlatformTheme::IconThemeSearchPaths:
        return QVariant(QGenericUnixTheme::xdgIconThemePaths());
    case QPlatformTheme::StyleNames: {
        QStringList styleNames;
        styleNames << QStringLiteral("GTK+") << QStringLiteral("cleanlooks") << QStringLiteral("windows");
        return QVariant(styleNames);
    }
    case QPlatformTheme::KeyboardScheme:
        return QVariant(int(GnomeKeyboardScheme));
    default:
        break;
    }
    return QPlatformTheme::themeHint(hint);
}

const QFont *QGnomeTheme::font(Font type) const
{
    if (type == QPlatformTheme::SystemFont)
        return &m_systemFont;
    return 0;
}

/*!
    \brief Creates a UNIX theme according to the detected desktop environment.
*/

QPlatformTheme *QGenericUnixTheme::createUnixTheme(const QString &name)
{
    if (name == QLatin1String(QGenericUnixTheme::name))
        return new QGenericUnixTheme;
#ifndef QT_NO_SETTINGS
    if (name == QLatin1String(QKdeTheme::name))
        if (QPlatformTheme *kdeTheme = QKdeTheme::createKdeTheme())
            return kdeTheme;
#endif
    if (name == QLatin1String(QGnomeTheme::name))
        return new QGnomeTheme;
    return new QGenericUnixTheme;
}

QStringList QGenericUnixTheme::themeNames()
{
    QStringList result;
    if (QGuiApplication::desktopSettingsAware()) {
        const QByteArray desktopEnvironment = QGuiApplicationPrivate::platformIntegration()->services()->desktopEnvironment();
        if (desktopEnvironment == QByteArrayLiteral("KDE")) {
#ifndef QT_NO_SETTINGS
            result.push_back(QLatin1String(QKdeTheme::name));
#endif
        } else { // Gnome, Unity, other Gtk-based desktops like XFCE.
            result.push_back(QLatin1String(QGnomeTheme::name));
        }
        const QString session = QString::fromLocal8Bit(qgetenv("DESKTOP_SESSION"));
        if (!session.isEmpty() && session != QStringLiteral("default") && !result.contains(session))
            result.push_back(session);
    } // desktopSettingsAware
    if (result.isEmpty())
        result.push_back(QLatin1String(QGenericUnixTheme::name));
    return result;
}

QT_END_NAMESPACE
