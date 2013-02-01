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

#include "qaccessiblewidgets.h"
#include "qabstracttextdocumentlayout.h"
#include "qapplication.h"
#include "qclipboard.h"
#include "qtextedit.h"
#include "private/qtextedit_p.h"
#include "qtextdocument.h"
#include "qtextobject.h"
#include "qplaintextedit.h"
#include "qtextboundaryfinder.h"
#include "qscrollbar.h"
#include "qdebug.h"
#include <QApplication>
#include <QStackedWidget>
#include <QToolBox>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QDialogButtonBox>
#include <limits.h>
#include <QRubberBand>
#include <QTextBrowser>
#include <QCalendarWidget>
#include <QAbstractItemView>
#include <QDockWidget>
#include <QMainWindow>
#include <QAbstractButton>
#include <private/qdockwidget_p.h>
#include <QFocusFrame>

#ifndef QT_NO_ACCESSIBILITY

QT_BEGIN_NAMESPACE

using namespace QAccessible2;

QString Q_GUI_EXPORT qt_accStripAmp(const QString &text);
QString Q_GUI_EXPORT qt_accHotKey(const QString &text);

QList<QWidget*> childWidgets(const QWidget *widget, bool includeTopLevel)
{
    if (widget == 0)
        return QList<QWidget*>();
    QList<QObject*> list = widget->children();
    QList<QWidget*> widgets;
    for (int i = 0; i < list.size(); ++i) {
        QWidget *w = qobject_cast<QWidget *>(list.at(i));
        if (!w)
            continue;
        QString objectName = w->objectName();
        if ((includeTopLevel || !w->isWindow())
              && !qobject_cast<QFocusFrame*>(w)
              && !qobject_cast<QMenu*>(w)
              && objectName != QLatin1String("qt_rubberband")
              && objectName != QLatin1String("qt_qmainwindow_extended_splitter")) {
            widgets.append(w);
        }
    }
    return widgets;
}

#if !defined(QT_NO_TEXTEDIT) && !defined(QT_NO_CURSOR)

QAccessiblePlainTextEdit::QAccessiblePlainTextEdit(QWidget* o)
  :QAccessibleTextWidget(o)
{
    Q_ASSERT(widget()->inherits("QPlainTextEdit"));
}

QPlainTextEdit* QAccessiblePlainTextEdit::plainTextEdit() const
{
    return static_cast<QPlainTextEdit *>(widget());
}

QString QAccessiblePlainTextEdit::text(QAccessible::Text t) const
{
    if (t == QAccessible::Value)
        return plainTextEdit()->toPlainText();

    return QAccessibleWidget::text(t);
}

void QAccessiblePlainTextEdit::setText(QAccessible::Text t, const QString &text)
{
    if (t != QAccessible::Value) {
        QAccessibleWidget::setText(t, text);
        return;
    }
    if (plainTextEdit()->isReadOnly())
        return;

    plainTextEdit()->setPlainText(text);
}

QAccessible::State QAccessiblePlainTextEdit::state() const
{
    QAccessible::State st = QAccessibleTextWidget::state();
    if (plainTextEdit()->isReadOnly())
        st.readOnly = true;
    else
        st.editable = true;
    return st;
}

void *QAccessiblePlainTextEdit::interface_cast(QAccessible::InterfaceType t)
{
    if (t == QAccessible::TextInterface)
        return static_cast<QAccessibleTextInterface*>(this);
    return QAccessibleWidget::interface_cast(t);
}

QPoint QAccessiblePlainTextEdit::scrollBarPosition() const
{
    QPoint result;
    result.setX(plainTextEdit()->horizontalScrollBar() ? plainTextEdit()->horizontalScrollBar()->sliderPosition() : 0);
    result.setY(plainTextEdit()->verticalScrollBar() ? plainTextEdit()->verticalScrollBar()->sliderPosition() : 0);
    return result;
}

QTextCursor QAccessiblePlainTextEdit::textCursor() const
{
    return plainTextEdit()->textCursor();
}

void QAccessiblePlainTextEdit::setTextCursor(const QTextCursor &textCursor)
{
    plainTextEdit()->setTextCursor(textCursor);
}

QTextDocument* QAccessiblePlainTextEdit::textDocument() const
{
    return plainTextEdit()->document();
}

QWidget* QAccessiblePlainTextEdit::viewport() const
{
    return plainTextEdit()->viewport();
}

void QAccessiblePlainTextEdit::scrollToSubstring(int startIndex, int endIndex)
{
    //TODO: Not implemented
    Q_UNUSED(startIndex);
    Q_UNUSED(endIndex);
}


/*!
  \class QAccessibleTextEdit
  \brief The QAccessibleTextEdit class implements the QAccessibleInterface for richtext editors.
  \internal
*/

/*!
  \fn QAccessibleTextEdit::QAccessibleTextEdit(QWidget *widget)

  Constructs a QAccessibleTextEdit object for a \a widget.
*/
QAccessibleTextEdit::QAccessibleTextEdit(QWidget *o)
: QAccessibleTextWidget(o, QAccessible::EditableText)
{
    Q_ASSERT(widget()->inherits("QTextEdit"));
}

/*! Returns the text edit. */
QTextEdit *QAccessibleTextEdit::textEdit() const
{
    return static_cast<QTextEdit *>(widget());
}

QTextCursor QAccessibleTextEdit::textCursor() const
{
    return textEdit()->textCursor();
}

QTextDocument *QAccessibleTextEdit::textDocument() const
{
    return textEdit()->document();
}

void QAccessibleTextEdit::setTextCursor(const QTextCursor &textCursor)
{
    textEdit()->setTextCursor(textCursor);
}

QWidget *QAccessibleTextEdit::viewport() const
{
    return textEdit()->viewport();
}

QPoint QAccessibleTextEdit::scrollBarPosition() const
{
    QPoint result;
    result.setX(textEdit()->horizontalScrollBar() ? textEdit()->horizontalScrollBar()->sliderPosition() : 0);
    result.setY(textEdit()->verticalScrollBar() ? textEdit()->verticalScrollBar()->sliderPosition() : 0);
    return result;
}

QString QAccessibleTextEdit::text(QAccessible::Text t) const
{
    if (t == QAccessible::Value)
        return textEdit()->toPlainText();

    return QAccessibleWidget::text(t);
}

void QAccessibleTextEdit::setText(QAccessible::Text t, const QString &text)
{
    if (t != QAccessible::Value) {
        QAccessibleWidget::setText(t, text);
        return;
    }
    if (textEdit()->isReadOnly())
        return;

    textEdit()->setText(text);
}

QAccessible::State QAccessibleTextEdit::state() const
{
    QAccessible::State st = QAccessibleTextWidget::state();
    if (textEdit()->isReadOnly())
        st.readOnly = true;
    else
        st.editable = true;
    return st;
}

void *QAccessibleTextEdit::interface_cast(QAccessible::InterfaceType t)
{
    if (t == QAccessible::TextInterface)
        return static_cast<QAccessibleTextInterface*>(this);
    return QAccessibleWidget::interface_cast(t);
}

void QAccessibleTextEdit::scrollToSubstring(int startIndex, int endIndex)
{
    QTextEdit *edit = textEdit();

    QTextCursor cursor = textCursor();
    cursor.setPosition(startIndex);
    QRect r = edit->cursorRect(cursor);

    cursor.setPosition(endIndex);
    r.setBottomRight(edit->cursorRect(cursor).bottomRight());

    r.moveTo(r.x() + edit->horizontalScrollBar()->value(),
             r.y() + edit->verticalScrollBar()->value());

    // E V I L, but ensureVisible is not public
    if (!QMetaObject::invokeMethod(edit, "_q_ensureVisible", Q_ARG(QRectF, r)))
        qWarning("AccessibleTextEdit::scrollToSubstring failed!");
}

#endif // QT_NO_TEXTEDIT && QT_NO_CURSOR

#ifndef QT_NO_STACKEDWIDGET
// ======================= QAccessibleStackedWidget ======================
QAccessibleStackedWidget::QAccessibleStackedWidget(QWidget *widget)
    : QAccessibleWidget(widget, QAccessible::LayeredPane)
{
    Q_ASSERT(qobject_cast<QStackedWidget *>(widget));
}

QAccessibleInterface *QAccessibleStackedWidget::childAt(int x, int y) const
{
    if (!stackedWidget()->isVisible())
        return 0;
    QWidget *currentWidget = stackedWidget()->currentWidget();
    if (!currentWidget)
        return 0;
    QPoint position = currentWidget->mapFromGlobal(QPoint(x, y));
    if (currentWidget->rect().contains(position))
        return child(stackedWidget()->currentIndex());
    return 0;
}

int QAccessibleStackedWidget::childCount() const
{
    return stackedWidget()->count();
}

int QAccessibleStackedWidget::indexOfChild(const QAccessibleInterface *child) const
{
    if (!child)
        return -1;

    QWidget *widget = qobject_cast<QWidget*>(child->object());
    return stackedWidget()->indexOf(widget);
}

QAccessibleInterface *QAccessibleStackedWidget::child(int index) const
{
    if (index < 0 || index >= stackedWidget()->count())
        return 0;
    return QAccessible::queryAccessibleInterface(stackedWidget()->widget(index));
}

QStackedWidget *QAccessibleStackedWidget::stackedWidget() const
{
    return static_cast<QStackedWidget *>(object());
}
#endif // QT_NO_STACKEDWIDGET

#ifndef QT_NO_TOOLBOX
// ======================= QAccessibleToolBox ======================
QAccessibleToolBox::QAccessibleToolBox(QWidget *widget)
    : QAccessibleWidget(widget, QAccessible::LayeredPane)
{
    Q_ASSERT(qobject_cast<QToolBox *>(widget));
}

QToolBox * QAccessibleToolBox::toolBox() const
{
    return static_cast<QToolBox *>(object());
}
#endif // QT_NO_TOOLBOX

// ======================= QAccessibleMdiArea ======================
#ifndef QT_NO_MDIAREA
QAccessibleMdiArea::QAccessibleMdiArea(QWidget *widget)
    : QAccessibleWidget(widget, QAccessible::LayeredPane)
{
    Q_ASSERT(qobject_cast<QMdiArea *>(widget));
}

int QAccessibleMdiArea::childCount() const
{
    return mdiArea()->subWindowList().count();
}

QAccessibleInterface *QAccessibleMdiArea::child(int index) const
{
    QList<QMdiSubWindow *> subWindows = mdiArea()->subWindowList();
    QWidget *targetObject = subWindows.value(index);
    if (!targetObject)
       return 0;
    return QAccessible::queryAccessibleInterface(targetObject);
}


int QAccessibleMdiArea::indexOfChild(const QAccessibleInterface *child) const
{
    if (!child || !child->object() || mdiArea()->subWindowList().isEmpty())
        return -1;
    if (QMdiSubWindow *window = qobject_cast<QMdiSubWindow *>(child->object())) {
        return mdiArea()->subWindowList().indexOf(window);
    }
    return -1;
}

QMdiArea *QAccessibleMdiArea::mdiArea() const
{
    return static_cast<QMdiArea *>(object());
}

// ======================= QAccessibleMdiSubWindow ======================
QAccessibleMdiSubWindow::QAccessibleMdiSubWindow(QWidget *widget)
    : QAccessibleWidget(widget, QAccessible::Window)
{
    Q_ASSERT(qobject_cast<QMdiSubWindow *>(widget));
}

QString QAccessibleMdiSubWindow::text(QAccessible::Text textType) const
{
    if (textType == QAccessible::Name) {
        QString title = mdiSubWindow()->windowTitle();
        title.replace(QLatin1String("[*]"), QLatin1String(""));
        return title;
    }
    return QAccessibleWidget::text(textType);
}

void QAccessibleMdiSubWindow::setText(QAccessible::Text textType, const QString &text)
{
    if (textType == QAccessible::Name)
        mdiSubWindow()->setWindowTitle(text);
    else
        QAccessibleWidget::setText(textType, text);
}

QAccessible::State QAccessibleMdiSubWindow::state() const
{
    QAccessible::State state;
    state.focusable = true;
    if (!mdiSubWindow()->isMaximized()) {
        state.movable = true;
        state.sizeable = true;
    }
    if (mdiSubWindow()->isAncestorOf(QApplication::focusWidget())
            || QApplication::focusWidget() == mdiSubWindow())
        state.focused = true;
    if (!mdiSubWindow()->isVisible())
        state.invisible = true;
    if (!mdiSubWindow()->parentWidget()->contentsRect().contains(mdiSubWindow()->geometry()))
        state.offscreen = true;
    if (!mdiSubWindow()->isEnabled())
        state.disabled = true;
    return state;
}

int QAccessibleMdiSubWindow::childCount() const
{
    if (mdiSubWindow()->widget())
        return 1;
    return 0;
}

QAccessibleInterface *QAccessibleMdiSubWindow::child(int index) const
{
    QMdiSubWindow *source = mdiSubWindow();
    if (index != 0 || !source->widget())
        return 0;

    return QAccessible::queryAccessibleInterface(source->widget());
}

int QAccessibleMdiSubWindow::indexOfChild(const QAccessibleInterface *child) const
{
    if (child && child->object() && child->object() == mdiSubWindow()->widget())
        return 0;
    return -1;
}

QRect QAccessibleMdiSubWindow::rect() const
{
    if (mdiSubWindow()->isHidden())
        return QRect();
    if (!mdiSubWindow()->parent())
        return QAccessibleWidget::rect();
    const QPoint pos = mdiSubWindow()->mapToGlobal(QPoint(0, 0));
    return QRect(pos, mdiSubWindow()->size());
}

QMdiSubWindow *QAccessibleMdiSubWindow::mdiSubWindow() const
{
    return static_cast<QMdiSubWindow *>(object());
}
#endif // QT_NO_MDIAREA

#ifndef QT_NO_DIALOGBUTTONBOX
// ======================= QAccessibleDialogButtonBox ======================
QAccessibleDialogButtonBox::QAccessibleDialogButtonBox(QWidget *widget)
    : QAccessibleWidget(widget, QAccessible::Grouping)
{
    Q_ASSERT(qobject_cast<QDialogButtonBox*>(widget));
}

#endif // QT_NO_DIALOGBUTTONBOX

#if !defined(QT_NO_TEXTBROWSER) && !defined(QT_NO_CURSOR)
QAccessibleTextBrowser::QAccessibleTextBrowser(QWidget *widget)
    : QAccessibleTextEdit(widget)
{
    Q_ASSERT(qobject_cast<QTextBrowser *>(widget));
}

QAccessible::Role QAccessibleTextBrowser::role() const
{
    return QAccessible::StaticText;
}
#endif // QT_NO_TEXTBROWSER && QT_NO_CURSOR

#ifndef QT_NO_CALENDARWIDGET
// ===================== QAccessibleCalendarWidget ========================
QAccessibleCalendarWidget::QAccessibleCalendarWidget(QWidget *widget)
    : QAccessibleWidget(widget, QAccessible::Table)
{
    Q_ASSERT(qobject_cast<QCalendarWidget *>(widget));
}

int QAccessibleCalendarWidget::childCount() const
{
   return calendarWidget()->isNavigationBarVisible() ? 2 : 1;
}

int QAccessibleCalendarWidget::indexOfChild(const QAccessibleInterface *child) const
{
    if (!child || !child->object() || childCount() <= 0)
        return -1;
    if (qobject_cast<QAbstractItemView *>(child->object()))
        return childCount() - 1; // FIXME
    return 0;
}

QAccessibleInterface *QAccessibleCalendarWidget::child(int index) const
{
    if (index < 0 || index >= childCount())
        return 0;

    if (childCount() > 1 && index == 0)
        return QAccessible::queryAccessibleInterface(navigationBar());

    return QAccessible::queryAccessibleInterface(calendarView());
}

QCalendarWidget *QAccessibleCalendarWidget::calendarWidget() const
{
    return static_cast<QCalendarWidget *>(object());
}

QAbstractItemView *QAccessibleCalendarWidget::calendarView() const
{
    foreach (QObject *child, calendarWidget()->children()) {
        if (child->objectName() == QLatin1String("qt_calendar_calendarview"))
            return static_cast<QAbstractItemView *>(child);
    }
    return 0;
}

QWidget *QAccessibleCalendarWidget::navigationBar() const
{
    foreach (QObject *child, calendarWidget()->children()) {
        if (child->objectName() == QLatin1String("qt_calendar_navigationbar"))
            return static_cast<QWidget *>(child);
    }
    return 0;
}
#endif // QT_NO_CALENDARWIDGET

#ifndef QT_NO_DOCKWIDGET
QAccessibleDockWidget::QAccessibleDockWidget(QWidget *widget)
    : QAccessibleWidget(widget, QAccessible::Window)
{

}

QAccessibleInterface *QAccessibleDockWidget::child(int index) const
{
    if (index == 0) {
        return new QAccessibleTitleBar(dockWidget());
    } else if (index == 1 && dockWidget()->widget()) {
        return QAccessible::queryAccessibleInterface(dockWidget()->widget());
    }
    return 0;
}

int QAccessibleDockWidget::childCount() const
{
    return dockWidget()->widget() ? 2 : 1;
}

int QAccessibleDockWidget::indexOfChild(const QAccessibleInterface *child) const
{
    if (child) {
        if (child->role() == QAccessible::TitleBar) {
            return 0;
        } else {
            return 1;   // FIXME
        }
    }
    return -1;
}

QRect QAccessibleDockWidget::rect() const
{
    QRect rect;

    if (dockWidget()->isFloating()) {
        rect = dockWidget()->frameGeometry();
    } else {
        rect = dockWidget()->rect();
        rect.moveTopLeft(dockWidget()->mapToGlobal(rect.topLeft()));
    }

    return rect;
}

QDockWidget *QAccessibleDockWidget::dockWidget() const
{
    return static_cast<QDockWidget *>(object());
}

////
//      QAccessibleTitleBar
////
QAccessibleTitleBar::QAccessibleTitleBar(QDockWidget *widget)
    : m_dockWidget(widget)
{

}

QAccessibleInterface *QAccessibleTitleBar::parent() const
{
    return new QAccessibleDockWidget(dockWidget());
}

QAccessibleInterface *QAccessibleTitleBar::child(int index) const
{
    if (index >= 0) {
        QDockWidgetLayout *layout = dockWidgetLayout();
        int role;
        int currentIndex = 0;
        for (role = QDockWidgetLayout::CloseButton; role <= QDockWidgetLayout::FloatButton; ++role) {
            QWidget *w = layout->widgetForRole((QDockWidgetLayout::Role)role);
            if (!w || !w->isVisible())
                continue;
            if (currentIndex == index)
                return QAccessible::queryAccessibleInterface(w);
            ++currentIndex;
        }
    }
    return 0;
}

int QAccessibleTitleBar::indexOfChild(const QAccessibleInterface * /*child*/) const
{
    return -1;
}

int QAccessibleTitleBar::childCount() const
{
    QDockWidgetLayout *layout = dockWidgetLayout();
    int count = 0;
    for (int role = QDockWidgetLayout::CloseButton; role <= QDockWidgetLayout::FloatButton; ++role) {
        QWidget *w = layout->widgetForRole((QDockWidgetLayout::Role)role);
        if (w && w->isVisible())
            ++count;
    }
    return count;
}

QString QAccessibleTitleBar::text(QAccessible::Text t) const
{
    if (t == QAccessible::Name || t == QAccessible::Value) {
        return qt_accStripAmp(dockWidget()->windowTitle());
    }
    return QString();
}

QAccessible::State QAccessibleTitleBar::state() const
{
    QAccessible::State state;

    QDockWidget *w = dockWidget();
    if (w->testAttribute(Qt::WA_WState_Visible) == false)
        state.invisible = true;
    if (w->focusPolicy() != Qt::NoFocus && w->isActiveWindow())
        state.focusable = true;
    if (w->hasFocus())
        state.focused = true;
    if (!w->isEnabled())
        state.disabled = true;

    return state;
}

QRect QAccessibleTitleBar::rect() const
{
    bool mapToGlobal = true;
    QRect rect;

    if (dockWidget()->isFloating()) {
        rect = dockWidget()->frameGeometry();
        if (dockWidget()->widget()) {
            QPoint globalPos = dockWidget()->mapToGlobal(dockWidget()->widget()->rect().topLeft());
            globalPos.ry()--;
            rect.setBottom(globalPos.y());
            mapToGlobal = false;
        }
    } else {
        QDockWidgetLayout *layout = qobject_cast<QDockWidgetLayout*>(dockWidget()->layout());
        rect = layout->titleArea();
    }

    if (rect.isNull())
        return rect;

    if (mapToGlobal)
        rect.moveTopLeft(dockWidget()->mapToGlobal(rect.topLeft()));
    return rect;
}

QAccessibleInterface *QAccessibleTitleBar::childAt(int x, int y) const
{
    for (int i = 0; i < childCount(); ++i) {
        QAccessibleInterface *childIface = child(i);
        if (childIface->rect().contains(x,y)) {
            return childIface;
        }
        delete childIface;
    }
    return 0;
}

QObject *QAccessibleTitleBar::object() const
{
    return 0;
}

QDockWidgetLayout *QAccessibleTitleBar::dockWidgetLayout() const
{
    return qobject_cast<QDockWidgetLayout*>(dockWidget()->layout());
}

QDockWidget *QAccessibleTitleBar::dockWidget() const
{
    return m_dockWidget;
}

//QString QAccessibleTitleBar::actionText(int action, Text t, int child) const
//{
//    QString str;
//    if (child >= 1 && child <= childCount()) {
//        if (t == Name) {
//            switch (action) {
//            case Press:
//            case DefaultAction:
//                if (child == QDockWidgetLayout::CloseButton) {
//                    str = QDockWidget::tr("Close");
//                } else if (child == QDockWidgetLayout::FloatButton) {
//                    str = dockWidget()->isFloating() ? QDockWidget::tr("Dock")
//                                                     : QDockWidget::tr("Float");
//                }
//                break;
//            default:
//                break;
//            }
//        }
//    }
//    return str;
//}

//bool QAccessibleTitleBar::doAction(int action, int child, const QVariantList& /*params*/)
//{
//    if (!child || !dockWidget()->isEnabled())
//        return false;

//    switch (action) {
//    case DefaultAction:
//    case Press: {
//        QDockWidgetLayout *layout = dockWidgetLayout();
//        QAbstractButton *btn = static_cast<QAbstractButton *>(layout->widgetForRole((QDockWidgetLayout::Role)child));
//        if (btn)
//            btn->animateClick();
//        return true;
//        break;}
//    default:
//        break;
//    }

//    return false;
//}

QAccessible::Role QAccessibleTitleBar::role() const
{
    return QAccessible::TitleBar;
}

void QAccessibleTitleBar::setText(QAccessible::Text /*t*/, const QString &/*text*/)
{
}

bool QAccessibleTitleBar::isValid() const
{
    return dockWidget();
}

#endif // QT_NO_DOCKWIDGET

#ifndef QT_NO_CURSOR

QAccessibleTextWidget::QAccessibleTextWidget(QWidget *o, QAccessible::Role r, const QString &name):
    QAccessibleWidget(o, r, name)
{

}

QAccessible::State QAccessibleTextWidget::state() const
{
    QAccessible::State s = QAccessibleWidget::state();
    s.multiLine = true;
    return s;
}

QRect QAccessibleTextWidget::characterRect(int offset) const
{
    QTextBlock block = textDocument()->findBlock(offset);
    if (!block.isValid())
        return QRect();

    QTextLayout *layout = block.layout();
    QPointF layoutPosition = layout->position();
    int relativeOffset = offset - block.position();
    QTextLine line = layout->lineForTextPosition(relativeOffset);

    QRect r;

    if (line.isValid()) {
        qreal x = line.cursorToX(relativeOffset);
        QFontMetrics fm(textCursor().charFormat().font());
        const QString ch = text(offset, offset + 1);
        if (!ch.isEmpty()) {
            int w = fm.width(ch);
            int h = fm.height();
            r = QRect(layoutPosition.x() + x, layoutPosition.y() + line.y(),
                      w, h);
            r.moveTo(viewport()->mapToGlobal(r.topLeft()));
        }
        r.translate(-scrollBarPosition());
    }

    return r;
}

int QAccessibleTextWidget::offsetAtPoint(const QPoint &point) const
{
    QPoint p = viewport()->mapFromGlobal(point);
    // convert to document coordinates
    p += scrollBarPosition();
    return textDocument()->documentLayout()->hitTest(p, Qt::ExactHit);
}

int QAccessibleTextWidget::selectionCount() const
{
    return textCursor().hasSelection() ? 1 : 0;
}

QString QAccessibleTextWidget::attributes(int offset, int *startOffset, int *endOffset) const
{
    /* The list of attributes can be found at:
     http://linuxfoundation.org/collaborate/workgroups/accessibility/iaccessible2/textattributes
    */

    if (offset >= characterCount()) {
        *startOffset = -1;
        *endOffset = -1;
        return QString();
    }

    QMap<QString, QString> attrs;

    QTextCursor cursor = textCursor();

    //cursor.charFormat returns the format of the previous character
    cursor.setPosition(offset + 1);
    QTextCharFormat charFormat = cursor.charFormat();

    cursor.setPosition(offset);
    QTextBlockFormat blockFormat = cursor.blockFormat();

    QTextCharFormat charFormatComp;
    QTextBlockFormat blockFormatComp;

    *startOffset = offset;
    cursor.setPosition(*startOffset);
    while (*startOffset > 0) {
        charFormatComp = cursor.charFormat();
        cursor.setPosition(*startOffset - 1);
        blockFormatComp = cursor.blockFormat();
        if ((charFormat == charFormatComp) && (blockFormat == blockFormatComp))
            (*startOffset)--;
        else
            break;
    }

    int limit = characterCount() + 1;
    *endOffset = offset + 1;
    cursor.setPosition(*endOffset);
    while (*endOffset < limit) {
        blockFormatComp = cursor.blockFormat();
        cursor.setPosition(*endOffset + 1);
        charFormatComp = cursor.charFormat();
        if ((charFormat == charFormatComp) && (cursor.blockFormat() == blockFormatComp))
            (*endOffset)++;
        else
            break;
    }

    QString family = charFormat.fontFamily();
    if (!family.isEmpty()) {
        family = family.replace('\\',"\\\\");
        family = family.replace(':',"\\:");
        family = family.replace(',',"\\,");
        family = family.replace('=',"\\=");
        family = family.replace(';',"\\;");
        family = family.replace('\"',"\\\"");
        attrs["font-family"] = '"'+family+'"';
    }

    int fontSize = int(charFormat.fontPointSize());
    if (fontSize)
        attrs["font-size"] = QString::number(fontSize).append("pt");

    //Different weight values are not handled
    attrs["font-weight"] = (charFormat.fontWeight() > QFont::Normal) ? "bold" : "normal";

    QFont::Style style = charFormat.font().style();
    attrs["font-style"] = (style == QFont::StyleItalic) ? "italic" : ((style == QFont::StyleOblique) ? "oblique": "normal");

    attrs["text-underline-style"] = charFormat.font().underline() ? "solid" : "none";

    QTextCharFormat::VerticalAlignment alignment = charFormat.verticalAlignment();
    attrs["text-position"] = (alignment == QTextCharFormat::AlignSubScript) ? "sub" : ((alignment == QTextCharFormat::AlignSuperScript) ? "super" : "baseline" );

    QBrush background = charFormat.background();
    if (background.style() == Qt::SolidPattern) {
        attrs["background-color"] = QString("rgb(%1,%2,%3)").arg(background.color().red()).arg(background.color().green()).arg(background.color().blue());
    }

    QBrush foreground = charFormat.foreground();
    if (foreground.style() == Qt::SolidPattern) {
        attrs["color"] = QString("rgb(%1,%2,%3)").arg(foreground.color().red()).arg(foreground.color().green()).arg(foreground.color().blue());
    }

    switch (blockFormat.alignment() & (Qt::AlignLeft | Qt::AlignRight | Qt::AlignHCenter | Qt::AlignJustify)) {
    case Qt::AlignLeft:
        attrs["text-align"] = "left";
        break;
    case Qt::AlignRight:
        attrs["text-align"] = "right";
        break;
    case Qt::AlignHCenter:
        attrs["text-align"] = "center";
        break;
    case Qt::AlignJustify:
        attrs["text-align"] = "left";
        break;
    }

    QString result;
    foreach (const QString &attributeName, attrs.keys()) {
        result.append(attributeName).append(':').append(attrs[attributeName]).append(';');
    }

    return result;
}

int QAccessibleTextWidget::cursorPosition() const
{
    return textCursor().position();
}

void QAccessibleTextWidget::selection(int selectionIndex, int *startOffset, int *endOffset) const
{
    *startOffset = *endOffset = 0;
    QTextCursor cursor = textCursor();

    if (selectionIndex != 0 || !cursor.hasSelection())
        return;

    *startOffset = cursor.selectionStart();
    *endOffset = cursor.selectionEnd();
}

QString QAccessibleTextWidget::text(int startOffset, int endOffset) const
{
    QTextCursor cursor(textCursor());

    cursor.setPosition(startOffset, QTextCursor::MoveAnchor);
    cursor.setPosition(endOffset, QTextCursor::KeepAnchor);

    return cursor.selectedText();
}

QPoint QAccessibleTextWidget::scrollBarPosition() const
{
    return QPoint(0, 0);
}

QPair< int, int > QAccessibleTextWidget::getBoundaries(int offset, BoundaryType boundaryType) const
{
    if (offset >= characterCount())
        return QPair<int, int>(characterCount(), characterCount());
    if (offset < 0)
        return QPair<int, int>(0, 0);

    QTextCursor cursor = textCursor();
    QPair<int, int> result;

    cursor.setPosition(offset);
    switch (boundaryType) {
    case CharBoundary:
        result.first = cursor.position();
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        result.second = cursor.position();
        break;
    case WordBoundary:
        cursor.movePosition(QTextCursor::StartOfWord, QTextCursor::MoveAnchor);
        result.first = cursor.position();
        cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
        result.second = cursor.position();
        break;
    case SentenceBoundary: {
        // QCursor does not provide functionality to move to next sentence.
        // We therefore find the current block, then go through the block using
        // QTextBoundaryFinder and find the sentence the \offset represents
        cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
        result.first = cursor.position();
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        result.second = cursor.position();
        QString blockText = cursor.selectedText();
        const int offsetWithinBlockText = offset - result.first;
        QTextBoundaryFinder sentenceFinder(QTextBoundaryFinder::Sentence, blockText);
        sentenceFinder.setPosition(offsetWithinBlockText);
        int prevBoundary = offsetWithinBlockText;
        int nextBoundary = offsetWithinBlockText;
        if (!(sentenceFinder.boundaryReasons() & QTextBoundaryFinder::StartOfItem))
            prevBoundary = sentenceFinder.toPreviousBoundary();
        nextBoundary = sentenceFinder.toNextBoundary();
        if (nextBoundary != -1)
            result.second = result.first + nextBoundary;
        if (prevBoundary != -1)
            result.first += prevBoundary;
        break; }
    case LineBoundary:
        cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::MoveAnchor);
        result.first = cursor.position();
        cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
        result.second = cursor.position();
        break;
    case ParagraphBoundary:
        cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
        result.first = cursor.position();
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        result.second = cursor.position();
        break;
    case NoBoundary:
        result.first = 0;
        result.second = characterCount();
        break;
    default:
        qWarning("QAccessibleTextWidget::getBoundaries: Unknown boundary type %d", boundaryType);
        result.first = -1;
        result.second = -1;
    }
    return result;
}

QString QAccessibleTextWidget::textBeforeOffset(int offset, BoundaryType boundaryType,
                                                int *startOffset, int *endOffset) const
{
    Q_ASSERT(startOffset);
    Q_ASSERT(endOffset);

    QPair<int, int> boundaries = getBoundaries(offset, boundaryType);
    boundaries = getBoundaries(boundaries.first - 1, boundaryType);

    *startOffset = boundaries.first;
    *endOffset = boundaries.second;

    return text(boundaries.first, boundaries.second);
 }


QString QAccessibleTextWidget::textAfterOffset(int offset, BoundaryType boundaryType,
                                              int *startOffset, int *endOffset) const
{
    Q_ASSERT(startOffset);
    Q_ASSERT(endOffset);

    QPair<int, int> boundaries = getBoundaries(offset, boundaryType);
    boundaries = getBoundaries(boundaries.second, boundaryType);

    *startOffset = boundaries.first;
    *endOffset = boundaries.second;

    return text(boundaries.first, boundaries.second);
}

QString QAccessibleTextWidget::textAtOffset(int offset, BoundaryType boundaryType,
                                            int *startOffset, int *endOffset) const
{
    Q_ASSERT(startOffset);
    Q_ASSERT(endOffset);

    QPair<int, int> boundaries = getBoundaries(offset, boundaryType);

    *startOffset = boundaries.first;
    *endOffset = boundaries.second;

    return text(boundaries.first, boundaries.second);
}

void QAccessibleTextWidget::setCursorPosition(int position)
{
    QTextCursor cursor = textCursor();
    cursor.setPosition(position);
    setTextCursor(cursor);
}

void QAccessibleTextWidget::addSelection(int startOffset, int endOffset)
{
    setSelection(0, startOffset, endOffset);
}

void QAccessibleTextWidget::removeSelection(int selectionIndex)
{
    if (selectionIndex != 0)
        return;

    QTextCursor cursor = textCursor();
    cursor.clearSelection();
    setTextCursor(cursor);
}

void QAccessibleTextWidget::setSelection(int selectionIndex, int startOffset, int endOffset)
{
    if (selectionIndex != 0)
        return;

    QTextCursor cursor = textCursor();
    cursor.setPosition(startOffset, QTextCursor::MoveAnchor);
    cursor.setPosition(endOffset, QTextCursor::KeepAnchor);
    setTextCursor(cursor);
}

int QAccessibleTextWidget::characterCount() const
{
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);
    return cursor.position();
}

QTextCursor QAccessibleTextWidget::textCursorForRange(int startOffset, int endOffset) const
{
    QTextCursor cursor = textCursor();
    cursor.setPosition(startOffset, QTextCursor::MoveAnchor);
    cursor.setPosition(endOffset, QTextCursor::KeepAnchor);

    return cursor;
}

void QAccessibleTextWidget::deleteText(int startOffset, int endOffset)
{
    QTextCursor cursor = textCursorForRange(startOffset, endOffset);
    cursor.removeSelectedText();
}

void QAccessibleTextWidget::insertText(int offset, const QString &text)
{
    QTextCursor cursor = textCursor();
    cursor.setPosition(offset);
    cursor.insertText(text);
}

void QAccessibleTextWidget::replaceText(int startOffset, int endOffset, const QString &text)
{
    QTextCursor cursor = textCursorForRange(startOffset, endOffset);
    cursor.removeSelectedText();
    cursor.insertText(text);
}
#endif // QT_NO_CURSOR


#ifndef QT_NO_MAINWINDOW
QAccessibleMainWindow::QAccessibleMainWindow(QWidget *widget)
    : QAccessibleWidget(widget, QAccessible::Window) { }

QAccessibleInterface *QAccessibleMainWindow::child(int index) const
{
    QList<QWidget*> kids = childWidgets(mainWindow(), true);
    if (index < kids.count()) {
        return QAccessible::queryAccessibleInterface(kids.at(index));
    }
    return 0;
}

int QAccessibleMainWindow::childCount() const
{
    QList<QWidget*> kids = childWidgets(mainWindow(), true);
    return kids.count();
}

int QAccessibleMainWindow::indexOfChild(const QAccessibleInterface *iface) const
{
    QList<QWidget*> kids = childWidgets(mainWindow(), true);
    return kids.indexOf(static_cast<QWidget*>(iface->object()));
}

QAccessibleInterface *QAccessibleMainWindow::childAt(int x, int y) const
{
    QWidget *w = widget();
    if (!w->isVisible())
        return 0;
    QPoint gp = w->mapToGlobal(QPoint(0, 0));
    if (!QRect(gp.x(), gp.y(), w->width(), w->height()).contains(x, y))
        return 0;

    QWidgetList kids = childWidgets(mainWindow(), true);
    QPoint rp = mainWindow()->mapFromGlobal(QPoint(x, y));
    for (int i = 0; i < kids.size(); ++i) {
        QWidget *child = kids.at(i);
        if (!child->isWindow() && !child->isHidden() && child->geometry().contains(rp)) {
            return QAccessible::queryAccessibleInterface(child);
        }
    }
    return 0;
}

QMainWindow *QAccessibleMainWindow::mainWindow() const
{
    return qobject_cast<QMainWindow *>(object());
}

#endif //QT_NO_MAINWINDOW

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY
