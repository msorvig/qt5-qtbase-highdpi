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

#include "qwindowsfontdatabase_ft.h"
#include "qwindowsfontdatabase.h"
#include "qwindowscontext.h"

#include <ft2build.h>
#include FT_TRUETYPE_TABLES_H

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QSettings>
#include <QtGui/private/qfontengine_ft_p.h>
#include <QtGui/QGuiApplication>
#include <QtGui/QFontDatabase>

#include <wchar.h>

QT_BEGIN_NAMESPACE

static inline QFontDatabase::WritingSystem writingSystemFromScript(const QString &scriptName)
{
    if (scriptName == QStringLiteral("Western")
            || scriptName == QStringLiteral("Baltic")
            || scriptName == QStringLiteral("Central European")
            || scriptName == QStringLiteral("Turkish")
            || scriptName == QStringLiteral("Vietnamese")
            || scriptName == QStringLiteral("OEM/Dos"))
        return QFontDatabase::Latin;
    if (scriptName == QStringLiteral("Thai"))
        return QFontDatabase::Thai;
    if (scriptName == QStringLiteral("Symbol")
        || scriptName == QStringLiteral("Other"))
        return QFontDatabase::Symbol;
    if (scriptName == QStringLiteral("CHINESE_GB2312"))
        return QFontDatabase::SimplifiedChinese;
    if (scriptName == QStringLiteral("CHINESE_BIG5"))
        return QFontDatabase::TraditionalChinese;
    if (scriptName == QStringLiteral("Cyrillic"))
        return QFontDatabase::Cyrillic;
    if (scriptName == QStringLiteral("Hangul"))
        return QFontDatabase::Korean;
    if (scriptName == QStringLiteral("Hebrew"))
        return QFontDatabase::Hebrew;
    if (scriptName == QStringLiteral("Greek"))
        return QFontDatabase::Greek;
    if (scriptName == QStringLiteral("Japanese"))
        return QFontDatabase::Japanese;
    if (scriptName == QStringLiteral("Arabic"))
        return QFontDatabase::Arabic;
    return QFontDatabase::Any;
}

// convert 0 ~ 1000 integer to QFont::Weight
static inline QFont::Weight weightFromInteger(long weight)
{
    if (weight < 400)
        return QFont::Light;
    if (weight < 600)
        return QFont::Normal;
    if (weight < 700)
        return QFont::DemiBold;
    if (weight < 800)
        return QFont::Bold;
    return QFont::Black;
}

static FontFile * createFontFile(const QString &fileName, int index)
{
    FontFile *fontFile = new FontFile;
    fontFile->fileName = fileName;
    fontFile->indexValue = index;
    return fontFile;
}

extern bool localizedName(const QString &name);
extern QString getEnglishName(const QString &familyName);

Q_GUI_EXPORT void qt_registerAliasToFontFamily(const QString &familyName, const QString &alias);

static bool addFontToDatabase(QString familyName, const QString &scriptName,
                              const TEXTMETRIC *textmetric,
                              const FONTSIGNATURE *signature,
                              int type)
{
    typedef QPair<QString, QStringList> FontKey;

    // the "@family" fonts are just the same as "family". Ignore them.
    if (familyName.at(0) == QLatin1Char('@') || familyName.startsWith(QStringLiteral("WST_")))
        return false;

    const int separatorPos = familyName.indexOf(QStringLiteral("::"));
    const QString faceName =
            separatorPos != -1 ? familyName.left(separatorPos) : familyName;
    const QString fullName =
            separatorPos != -1 ? familyName.mid(separatorPos + 2) : QString();
    static const int SMOOTH_SCALABLE = 0xffff;
    const QString foundryName; // No such concept.
    const NEWTEXTMETRIC *tm = (NEWTEXTMETRIC *)textmetric;
    const bool fixed = !(tm->tmPitchAndFamily & TMPF_FIXED_PITCH);
    const bool ttf = (tm->tmPitchAndFamily & TMPF_TRUETYPE);
    const bool scalable = tm->tmPitchAndFamily & (TMPF_VECTOR|TMPF_TRUETYPE);
    const int size = scalable ? SMOOTH_SCALABLE : tm->tmHeight;
    const QFont::Style style = tm->tmItalic ? QFont::StyleItalic : QFont::StyleNormal;
    const bool antialias = false;
    const QFont::Weight weight = weightFromInteger(tm->tmWeight);
    const QFont::Stretch stretch = QFont::Unstretched;

#ifndef QT_NO_DEBUG_OUTPUT
    if (QWindowsContext::verboseFonts > 2) {
        QDebug nospace = qDebug().nospace();
        nospace << __FUNCTION__ << faceName << fullName << scriptName
                 << "TTF=" << ttf;
        if (type & DEVICE_FONTTYPE)
            nospace << " DEVICE";
        if (type & RASTER_FONTTYPE)
            nospace << " RASTER";
        if (type & TRUETYPE_FONTTYPE)
            nospace << " TRUETYPE";
        nospace << " scalable=" << scalable << " Size=" << size
                << " Style=" << style << " Weight=" << weight
                << " stretch=" << stretch;
    }
#endif

    QString englishName;
    if (ttf && localizedName(faceName))
        englishName = getEnglishName(faceName);

    QSupportedWritingSystems writingSystems;
    if (type & TRUETYPE_FONTTYPE) {
        quint32 unicodeRange[4] = {
            signature->fsUsb[0], signature->fsUsb[1],
            signature->fsUsb[2], signature->fsUsb[3]
        };
        quint32 codePageRange[2] = {
            signature->fsCsb[0], signature->fsCsb[1]
        };
        writingSystems = QBasicFontDatabase::determineWritingSystemsFromTrueTypeBits(unicodeRange, codePageRange);
        // ### Hack to work around problem with Thai text on Windows 7. Segoe UI contains
        // the symbol for Baht, and Windows thus reports that it supports the Thai script.
        // Since it's the default UI font on this platform, most widgets will be unable to
        // display Thai text by default. As a temporary work around, we special case Segoe UI
        // and remove the Thai script from its list of supported writing systems.
        if (writingSystems.supported(QFontDatabase::Thai) &&
                faceName == QStringLiteral("Segoe UI"))
            writingSystems.setSupported(QFontDatabase::Thai, false);
    } else {
        const QFontDatabase::WritingSystem ws = writingSystemFromScript(scriptName);
        if (ws != QFontDatabase::Any)
            writingSystems.setSupported(ws);
    }

#ifndef Q_OS_WINCE
    const QSettings fontRegistry(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts"),
                                QSettings::NativeFormat);

    static QVector<FontKey> allFonts;
    if (allFonts.isEmpty()) {
        const QStringList allKeys = fontRegistry.allKeys();
        allFonts.reserve(allKeys.size());
        const QString trueType = QStringLiteral("(TrueType)");
        const QRegExp sizeListMatch(QStringLiteral("\\s(\\d+,)+\\d+"));
        foreach (const QString &key, allKeys) {
            QString realKey = key;
            realKey.remove(trueType);
            realKey.remove(sizeListMatch);
            QStringList fonts;
            const QStringList fontNames = realKey.trimmed().split(QLatin1Char('&'));
            foreach (const QString &fontName, fontNames)
                fonts.push_back(fontName.trimmed());
            allFonts.push_back(FontKey(key, fonts));
        }
    }

    QString value;
    int index = 0;
    for (int k = 0; k < allFonts.size(); ++k) {
        const FontKey &fontKey = allFonts.at(k);
        for (int i = 0; i < fontKey.second.length(); ++i) {
            const QString &font = fontKey.second.at(i);
            if (font == faceName || fullName == font || englishName == font) {
                value = fontRegistry.value(fontKey.first).toString();
                index = i;
                break;
            }
        }
        if (!value.isEmpty())
            break;
    }
#else
    QString value;
    int index = 0;

    static QHash<QString, QString> fontCache;

    if (fontCache.isEmpty()) {
        QSettings settings(QSettings::SystemScope, QStringLiteral("Qt-Project"), QStringLiteral("Qtbase"));
        settings.beginGroup(QStringLiteral("CEFontCache"));

        foreach (const QString &fontName, settings.allKeys()) {
            const QString fontFileName = settings.value(fontName).toString();
            fontCache.insert(fontName, fontFileName);
        }

        settings.endGroup(); // CEFontCache
    }

    value = fontCache.value(faceName);

    //Fallback if we havent cached the font yet or the font got removed/renamed iterate again over all fonts
    if (value.isEmpty() || !QFile::exists(value)) {
        QSettings settings(QSettings::SystemScope, QStringLiteral("Qt-Project"), QStringLiteral("Qtbase"));
        settings.beginGroup(QStringLiteral("CEFontCache"));

        //empty the cache first, as it seems that it is dirty
        foreach (const QString &fontName, settings.allKeys())
            settings.remove(fontName);

        QDirIterator it(QStringLiteral("/Windows"), QStringList(QStringLiteral("*.ttf")), QDir::Files | QDir::Hidden | QDir::System);

        while (it.hasNext()) {
            const QString fontFile = it.next();
            const QString fontName = QBasicFontDatabase::fontNameFromTTFile(fontFile);
            if (fontName.isEmpty())
                continue;
            fontCache.insert(fontName, fontFile);
            settings.setValue(fontName, fontFile);
        }

        value = fontCache.value(faceName);

        settings.endGroup(); // CEFontCache
    }
#endif

    if (value.isEmpty())
        return false;

    if (!QDir::isAbsolutePath(value))
        value.prepend(QString::fromLocal8Bit(qgetenv("windir") + "\\Fonts\\"));

    QPlatformFontDatabase::registerFont(faceName, QString(), foundryName, weight, style, stretch,
        antialias, scalable, size, fixed, writingSystems, createFontFile(value, index));

    // add fonts windows can generate for us:
    if (weight <= QFont::DemiBold)
        QPlatformFontDatabase::registerFont(faceName, QString(), foundryName, QFont::Bold, style, stretch,
                                            antialias, scalable, size, fixed, writingSystems, createFontFile(value, index));

    if (style != QFont::StyleItalic)
        QPlatformFontDatabase::registerFont(faceName, QString(), foundryName, weight, QFont::StyleItalic, stretch,
                                            antialias, scalable, size, fixed, writingSystems, createFontFile(value, index));

    if (weight <= QFont::DemiBold && style != QFont::StyleItalic)
        QPlatformFontDatabase::registerFont(faceName, QString(), foundryName, QFont::Bold, QFont::StyleItalic, stretch,
                                            antialias, scalable, size, fixed, writingSystems, createFontFile(value, index));

    if (!englishName.isEmpty())
        qt_registerAliasToFontFamily(faceName, englishName);

    return true;
}

static int CALLBACK storeFont(ENUMLOGFONTEX* f, NEWTEXTMETRICEX *textmetric,
                              int type, LPARAM namesSetIn)
{
    typedef QSet<QString> StringSet;
    const QString familyName = QString::fromWCharArray(f->elfLogFont.lfFaceName)
                               + QStringLiteral("::")
                               + QString::fromWCharArray(f->elfFullName);
    const QString script = QString::fromWCharArray(f->elfScript);

    const FONTSIGNATURE signature = textmetric->ntmFontSig;

    // NEWTEXTMETRICEX is a NEWTEXTMETRIC, which according to the documentation is
    // identical to a TEXTMETRIC except for the last four members, which we don't use
    // anyway
    if (addFontToDatabase(familyName, script, (TEXTMETRIC *)textmetric, &signature, type))
        reinterpret_cast<StringSet *>(namesSetIn)->insert(familyName);

    // keep on enumerating
    return 1;
}

void QWindowsFontDatabaseFT::populateFontDatabase()
{
    m_families.clear();
    populate(); // Called multiple times.
    // Work around EnumFontFamiliesEx() not listing the system font, see below.
    const QString sysFontFamily = QGuiApplication::font().family();
    if (!m_families.contains(sysFontFamily))
         populate(sysFontFamily);
}

/*!
    \brief Populate font database using EnumFontFamiliesEx().

    Normally, leaving the name empty should enumerate
    all fonts, however, system fonts like "MS Shell Dlg 2"
    are only found when specifying the name explicitly.
*/

void QWindowsFontDatabaseFT::populate(const QString &family)
    {

    if (QWindowsContext::verboseFonts)
        qDebug() << __FUNCTION__ << m_families.size() << family;

    HDC dummy = GetDC(0);
    LOGFONT lf;
    lf.lfCharSet = DEFAULT_CHARSET;
    if (family.size() >= LF_FACESIZE) {
        qWarning("%s: Unable to enumerate family '%s'.",
                 __FUNCTION__, qPrintable(family));
        return;
    }
    wmemcpy(lf.lfFaceName, reinterpret_cast<const wchar_t*>(family.utf16()),
            family.size() + 1);
    lf.lfPitchAndFamily = 0;
    EnumFontFamiliesEx(dummy, &lf, (FONTENUMPROC)storeFont,
                       (LPARAM)&m_families, 0);
    ReleaseDC(0, dummy);
}

QFontEngine * QWindowsFontDatabaseFT::fontEngine(const QFontDef &fontDef, QChar::Script script, void *handle)
{
    QFontEngine *fe = QBasicFontDatabase::fontEngine(fontDef, script, handle);
    if (QWindowsContext::verboseFonts)
        qDebug() << __FUNCTION__ << "FONTDEF" << /*fontDef <<*/ script << fe << handle;
    return fe;
}

QFontEngine *QWindowsFontDatabaseFT::fontEngine(const QByteArray &fontData, qreal pixelSize, QFont::HintingPreference hintingPreference)
{
    QFontEngine *fe = QBasicFontDatabase::fontEngine(fontData, pixelSize, hintingPreference);
    if (QWindowsContext::verboseFonts)
        qDebug() << __FUNCTION__ << "FONTDATA" << fontData << pixelSize << hintingPreference << fe;
    return fe;
}

static const char *other_tryFonts[] = {
    "Arial",
    "MS UI Gothic",
    "Gulim",
    "SimSun",
    "PMingLiU",
    "Arial Unicode MS",
    0
};

static const char *jp_tryFonts [] = {
    "MS UI Gothic",
    "Arial",
    "Gulim",
    "SimSun",
    "PMingLiU",
    "Arial Unicode MS",
    0
};

static const char *ch_CN_tryFonts [] = {
    "SimSun",
    "Arial",
    "PMingLiU",
    "Gulim",
    "MS UI Gothic",
    "Arial Unicode MS",
    0
};

static const char *ch_TW_tryFonts [] = {
    "PMingLiU",
    "Arial",
    "SimSun",
    "Gulim",
    "MS UI Gothic",
    "Arial Unicode MS",
    0
};

static const char *kr_tryFonts[] = {
    "Gulim",
    "Arial",
    "PMingLiU",
    "SimSun",
    "MS UI Gothic",
    "Arial Unicode MS",
    0
};

static const char **tryFonts = 0;

QStringList QWindowsFontDatabaseFT::fallbacksForFamily(const QString &family, QFont::Style style, QFont::StyleHint styleHint, QChar::Script script) const
{
    if (script == QChar::Script_Common) {
//            && !(request.styleStrategy & QFont::NoFontMerging)) {
        QFontDatabase db;
        if (!db.writingSystems(family).contains(QFontDatabase::Symbol)) {
            if(!tryFonts) {
                LANGID lid = GetUserDefaultLangID();
                switch( lid&0xff ) {
                case LANG_CHINESE: // Chinese (Taiwan)
                    if ( lid == 0x0804 ) // Taiwan
                        tryFonts = ch_TW_tryFonts;
                    else
                        tryFonts = ch_CN_tryFonts;
                    break;
                case LANG_JAPANESE:
                    tryFonts = jp_tryFonts;
                    break;
                case LANG_KOREAN:
                    tryFonts = kr_tryFonts;
                    break;
                default:
                    tryFonts = other_tryFonts;
                    break;
                }
            }
            QStringList list;
            const char **tf = tryFonts;
            while(tf && *tf) {
                if(m_families.contains(QLatin1String(*tf)))
                    list << QLatin1String(*tf);
                ++tf;
            }
            if (!list.isEmpty())
                return list;
        }
    }
    QStringList result = QPlatformFontDatabase::fallbacksForFamily(family, style, styleHint, script);
    if (!result.isEmpty())
        return result;

    switch (styleHint) {
        case QFont::Times:
            result << QString::fromLatin1("Times New Roman");
            break;
        case QFont::Courier:
            result << QString::fromLatin1("Courier New");
            break;
        case QFont::Monospace:
            result << QString::fromLatin1("Courier New");
            break;
        case QFont::Cursive:
            result << QString::fromLatin1("Comic Sans MS");
            break;
        case QFont::Fantasy:
            result << QString::fromLatin1("Impact");
            break;
        case QFont::Decorative:
            result << QString::fromLatin1("Old English");
            break;
        case QFont::Helvetica:
        case QFont::System:
        default:
            result << QString::fromLatin1("Arial");
    }
    if (QWindowsContext::verboseFonts)
        qDebug() << __FUNCTION__ << family << style << styleHint
                 << script << result << m_families;
    return result;
}
QString QWindowsFontDatabaseFT::fontDir() const
{
    const QString result = QLatin1String(qgetenv("windir")) + QLatin1String("/Fonts");//QPlatformFontDatabase::fontDir();
    if (QWindowsContext::verboseFonts)
        qDebug() << __FUNCTION__ << result;
    return result;
}

HFONT QWindowsFontDatabaseFT::systemFont()
{
    static const HFONT stock_sysfont = (HFONT)GetStockObject(SYSTEM_FONT);
    return stock_sysfont;
}

// Creation functions

static inline bool scriptRequiresOpenType(int script)
{
    return ((script >= QChar::Script_Syriac && script <= QChar::Script_Sinhala)
            || script == QChar::Script_Khmer || script == QChar::Script_Nko);
}

static inline int verticalDPI()
{
    return GetDeviceCaps(QWindowsContext::instance()->displayContext(), LOGPIXELSY);
}

QFont QWindowsFontDatabaseFT::defaultFont() const
{
    return QWindowsFontDatabase::systemDefaultFont();
}

QFont QWindowsFontDatabaseFT::LOGFONT_to_QFont(const LOGFONT& logFont, int verticalDPI_In)
{
    if (verticalDPI_In <= 0)
        verticalDPI_In = verticalDPI();
    QFont qFont(QString::fromWCharArray(logFont.lfFaceName));
    qFont.setItalic(logFont.lfItalic);
    if (logFont.lfWeight != FW_DONTCARE)
        qFont.setWeight(weightFromInteger(logFont.lfWeight));
    const qreal logFontHeight = qAbs(logFont.lfHeight);
    qFont.setPointSizeF(logFontHeight * 72.0 / qreal(verticalDPI_In));
    qFont.setUnderline(logFont.lfUnderline);
    qFont.setOverline(false);
    qFont.setStrikeOut(logFont.lfStrikeOut);
    return qFont;
}

QT_END_NAMESPACE
