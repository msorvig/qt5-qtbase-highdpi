/****************************************************************************
**
** Copyright (C) 2012 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author James Turner <james.turner@kdab.com>
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

#include "qcocoamenuitem.h"

#include "qcocoamenu.h"
#include "qcocoahelpers.h"
#include "qcocoaautoreleasepool.h"
#include "qt_mac_p.h"
#include "qcocoaapplication.h" // for custom application category
#include "qcocoamenuloader.h"

#include <QtCore/QDebug>

static inline QT_MANGLE_NAMESPACE(QCocoaMenuLoader) *getMenuLoader()
{
    return [NSApp QT_MANGLE_NAMESPACE(qt_qcocoamenuLoader)];
}


static quint32 constructModifierMask(quint32 accel_key)
{
    quint32 ret = 0;
    const bool dontSwap = qApp->testAttribute(Qt::AA_MacDontSwapCtrlAndMeta);
    if ((accel_key & Qt::CTRL) == Qt::CTRL)
        ret |= (dontSwap ? NSControlKeyMask : NSCommandKeyMask);
    if ((accel_key & Qt::META) == Qt::META)
        ret |= (dontSwap ? NSCommandKeyMask : NSControlKeyMask);
    if ((accel_key & Qt::ALT) == Qt::ALT)
        ret |= NSAlternateKeyMask;
    if ((accel_key & Qt::SHIFT) == Qt::SHIFT)
        ret |= NSShiftKeyMask;
    return ret;
}

// return an autoreleased string given a QKeySequence (currently only looks at the first one).
NSString *keySequenceToKeyEqivalent(const QKeySequence &accel)
{
    quint32 accel_key = (accel[0] & ~(Qt::MODIFIER_MASK | Qt::UNICODE_ACCEL));
    QChar cocoa_key = qt_mac_qtKey2CocoaKey(Qt::Key(accel_key));
    if (cocoa_key.isNull())
        cocoa_key = QChar(accel_key).toLower().unicode();
    return [NSString stringWithCharacters:&cocoa_key.unicode() length:1];
}

// return the cocoa modifier mask for the QKeySequence (currently only looks at the first one).
NSUInteger keySequenceModifierMask(const QKeySequence &accel)
{
    return constructModifierMask(accel[0]);
}

QCocoaMenuItem::QCocoaMenuItem() :
    m_native(NULL),
    m_menu(NULL),
    m_isVisible(true),
    m_enabled(true),
    m_isSeparator(false),
    m_role(NoRole),
    m_checked(false),
    m_merged(false),
    m_tag(0)
{
}

QCocoaMenuItem::~QCocoaMenuItem()
{
    if (m_merged) {
        [m_native setHidden:YES];
    }

    [m_native release];
}

void QCocoaMenuItem::setText(const QString &text)
{
    m_text = qt_mac_removeAmpersandEscapes(text);
}

void QCocoaMenuItem::setIcon(const QIcon &icon)
{
    m_icon = icon;
}

void QCocoaMenuItem::setMenu(QPlatformMenu *menu)
{
    if (menu == m_menu)
        return;

    QCocoaAutoReleasePool pool;
    m_menu = static_cast<QCocoaMenu *>(menu);
    if (m_menu) {
        m_menu->setParentItem(this);
    } else {
        // we previously had a menu, but no longer
        // clear out our item so the nexy sync() call builds a new one
        [m_native release];
        m_native = nil;
    }
}

void QCocoaMenuItem::setVisible(bool isVisible)
{
    m_isVisible = isVisible;
}

void QCocoaMenuItem::setIsSeparator(bool isSeparator)
{
    m_isSeparator = isSeparator;
}

void QCocoaMenuItem::setFont(const QFont &font)
{
    m_font = font;
}

void QCocoaMenuItem::setRole(MenuRole role)
{
    m_role = role;
}

void QCocoaMenuItem::setShortcut(const QKeySequence& shortcut)
{
    m_shortcut = shortcut;
}

void QCocoaMenuItem::setChecked(bool isChecked)
{
    m_checked = isChecked;
}

void QCocoaMenuItem::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

NSMenuItem *QCocoaMenuItem::sync()
{
    if (m_isSeparator != [m_native isSeparatorItem]) {
        [m_native release];
        if (m_isSeparator) {
            m_native = [[NSMenuItem separatorItem] retain];
            [m_native setTag:reinterpret_cast<NSInteger>(this)];
        } else
            m_native = nil;
    }

    if (m_menu) {
        if (m_native != m_menu->nsMenuItem()) {
            [m_native release];
            m_native = [m_menu->nsMenuItem() retain];
            [m_native setTag:reinterpret_cast<NSInteger>(this)];
        }
    }

    if ((m_role != NoRole) || m_merged) {
        NSMenuItem *mergeItem = nil;
        QT_MANGLE_NAMESPACE(QCocoaMenuLoader) *loader = getMenuLoader();
        switch (m_role) {
        case ApplicationSpecificRole:
            mergeItem = [loader appSpecificMenuItem];
            break;
        case AboutRole:
            mergeItem = [loader aboutMenuItem];
            break;
        case AboutQtRole:
            mergeItem = [loader aboutQtMenuItem];
            break;
        case QuitRole:
            mergeItem = [loader quitMenuItem];
            break;
        case PreferencesRole:
            mergeItem = [loader preferencesMenuItem];
            break;
        case TextHeuristicRole: {
            QString aboutString = tr("About").toLower();
            if (m_text.startsWith(aboutString, Qt::CaseInsensitive)
                || m_text.endsWith(aboutString, Qt::CaseInsensitive))
            {
                if (m_text.indexOf(QRegExp(QString::fromLatin1("qt$"), Qt::CaseInsensitive)) == -1)
                    mergeItem = [loader aboutMenuItem];
                else
                    mergeItem = [loader aboutQtMenuItem];

                m_merged = true;
            } else if (m_text.startsWith(tr("Config"), Qt::CaseInsensitive)
                       || m_text.startsWith(tr("Preference"), Qt::CaseInsensitive)
                       || m_text.startsWith(tr("Options"), Qt::CaseInsensitive)
                       || m_text.startsWith(tr("Setting"), Qt::CaseInsensitive)
                       || m_text.startsWith(tr("Setup"), Qt::CaseInsensitive)) {
                mergeItem = [loader preferencesMenuItem];
            } else if (m_text.startsWith(tr("Quit"), Qt::CaseInsensitive)
                       || m_text.startsWith(tr("Exit"), Qt::CaseInsensitive)) {
                mergeItem = [loader quitMenuItem];
            }

            break;
        }

        default:
            qWarning() << Q_FUNC_INFO << "unsupported role" << (int) m_role;
        }

        if (mergeItem) {
            m_merged = true;
            [m_native release];
            m_native = mergeItem;
            [m_native retain]; // balance out release!
            [m_native setTag:reinterpret_cast<NSInteger>(this)];
        } else if (m_merged) {
            // was previously merged, but no longer
            [m_native release];
            m_native = nil; // create item below
            m_merged = false;
        }
    }

    if (!m_native) {
        m_native = [[NSMenuItem alloc] initWithTitle:QCFString::toNSString(m_text)
            action:nil
                keyEquivalent:@""];
        [m_native retain];
        [m_native setTag:reinterpret_cast<NSInteger>(this)];
    }

//  [m_native setHidden:YES];
//  [m_native setHidden:NO];
   [m_native setHidden: !m_isVisible];
    [m_native setEnabled: m_enabled];
    QString text = m_text;
    QKeySequence accel = m_shortcut;

    {
        int st = text.lastIndexOf(QLatin1Char('\t'));
        if (st != -1) {
            accel = QKeySequence(text.right(text.length()-(st+1)));
            text.remove(st, text.length()-st);
        }
    }

    text = mergeText();
    accel = mergeAccel();

    // Show multiple key sequences as part of the menu text.
    if (accel.count() > 1)
        text += QLatin1String(" (") + accel.toString(QKeySequence::NativeText) + QLatin1String(")");

    QString finalString = qt_mac_removeMnemonics(text);
    // Cocoa Font and title
    if (m_font.resolve()) {
        NSFont *customMenuFont = [NSFont fontWithName:QCFString::toNSString(m_font.family())
                                  size:m_font.pointSize()];
        NSArray *keys = [NSArray arrayWithObjects:NSFontAttributeName, nil];
        NSArray *objects = [NSArray arrayWithObjects:customMenuFont, nil];
        NSDictionary *attributes = [NSDictionary dictionaryWithObjects:objects forKeys:keys];
        NSAttributedString *str = [[[NSAttributedString alloc] initWithString:QCFString::toNSString(finalString)
                                 attributes:attributes] autorelease];
       [m_native setAttributedTitle: str];
    } else {
       [m_native setTitle: QCFString::toNSString(finalString)];
    }

    if (accel.count() == 1) {
        [m_native setKeyEquivalent:keySequenceToKeyEqivalent(accel)];
        [m_native setKeyEquivalentModifierMask:keySequenceModifierMask(accel)];
    } else {
        [m_native setKeyEquivalent:@""];
        [m_native setKeyEquivalentModifierMask:NSCommandKeyMask];
    }

    if (!m_icon.isNull()) {
        NSImage *img = static_cast<NSImage *>(qt_mac_create_nsimage(m_icon.pixmap(16, QIcon::Normal)));
        [m_native setImage: img];
        [img release];
    }

    [m_native setState:m_checked ?  NSOnState : NSOffState];
    return m_native;
}

QT_BEGIN_NAMESPACE
extern QString qt_mac_applicationmenu_string(int type);
QT_END_NAMESPACE

QString QCocoaMenuItem::mergeText()
{
    QT_MANGLE_NAMESPACE(QCocoaMenuLoader) *loader = getMenuLoader();
    if (m_native == [loader aboutMenuItem]) {
        return qt_mac_applicationmenu_string(6).arg(qt_mac_applicationName());
    } else if (m_native== [loader aboutQtMenuItem]) {
        if (m_text == QString("About Qt"))
            return tr("About Qt");
        else
            return m_text;
    } else if (m_native == [loader preferencesMenuItem]) {
        return qt_mac_applicationmenu_string(4);
    } else if (m_native == [loader quitMenuItem]) {
        return qt_mac_applicationmenu_string(5).arg(qt_mac_applicationName());
    }
    return m_text;
}

QKeySequence QCocoaMenuItem::mergeAccel()
{
    QT_MANGLE_NAMESPACE(QCocoaMenuLoader) *loader = getMenuLoader();
    if (m_native == [loader preferencesMenuItem])
        return QKeySequence(QKeySequence::Preferences);
    else if (m_native == [loader quitMenuItem])
        return QKeySequence(QKeySequence::Quit);

    return m_shortcut;
}

void QCocoaMenuItem::syncMerged()
{
    if (!m_merged) {
        qWarning() << Q_FUNC_INFO << "Trying to sync a non-merged item";
        return;
    }
    [m_native setTag:reinterpret_cast<NSInteger>(this)];
    [m_native setHidden: !m_isVisible];
}

void QCocoaMenuItem::syncModalState(bool modal)
{
    if (modal)
        [m_native setEnabled:NO];
    else
        [m_native setEnabled:m_enabled];
}
