/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QACCESSIBLEOBJECT_H
#define QACCESSIBLEOBJECT_H

#include <QtGui/qaccessible.h>
#include <QtGui/qaccessible2.h>

QT_BEGIN_NAMESPACE


#ifndef QT_NO_ACCESSIBILITY

class QAccessibleObjectPrivate;
class QObject;

class Q_GUI_EXPORT QAccessibleObject : public QAccessibleInterface
{
public:
    explicit QAccessibleObject(QObject *object);

    bool isValid() const;
    QObject *object() const;

    // properties
    QRect rect() const;
    void setText(QAccessible::Text t, const QString &text);
    QAccessibleInterface *childAt(int x, int y) const;

protected:
    virtual ~QAccessibleObject();

private:
    QAccessibleObjectPrivate *d;
    Q_DISABLE_COPY(QAccessibleObject)
};

class Q_GUI_EXPORT QAccessibleApplication : public QAccessibleObject
{
public:
    QAccessibleApplication();

    QWindow *window() const;
    // relations
    int childCount() const;
    int indexOfChild(const QAccessibleInterface*) const;
    QAccessibleInterface *focusChild() const;

    // navigation
    QAccessibleInterface *parent() const;
    QAccessibleInterface *child(int index) const;

    // properties and state
    QString text(QAccessible::Text t) const;
    QAccessible::Role role() const;
    QAccessible::State state() const;
};

#endif // QT_NO_ACCESSIBILITY

QT_END_NAMESPACE

#endif // QACCESSIBLEOBJECT_H
