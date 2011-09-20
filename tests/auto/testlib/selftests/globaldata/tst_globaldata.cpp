/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/


#include <QtCore>
#include <QtTest/QtTest>
#include <QDebug>

class tst_globaldata: public QObject
{
    Q_OBJECT
public slots:
    void init();
    void initTestCase();
    void initTestCase_data();

    void cleanup();
    void cleanupTestCase();

private slots:
    void testGlobal_data();
    void testGlobal();

    void skip_data();
    void skip();

    void skipLocal_data() { testGlobal_data(); }
    void skipLocal();

    void skipSingle_data() { testGlobal_data(); }
    void skipSingle();
};


void tst_globaldata::initTestCase()
{
    qDebug() << "initTestCase"
             << (QTest::currentTestFunction() ? QTest::currentTestFunction() : "(null)")
             << (QTest::currentDataTag() ? QTest::currentDataTag() : "(null)");
}

void tst_globaldata::initTestCase_data()
{
    QTest::addColumn<bool>("booli");
    QTest::newRow("1") << false;
    QTest::newRow("2") << true;
}

void tst_globaldata::cleanupTestCase()
{
    qDebug() << "cleanupTestCase"
             << (QTest::currentTestFunction() ? QTest::currentTestFunction() : "(null)")
             << (QTest::currentDataTag() ? QTest::currentDataTag() : "(null)");
}

void tst_globaldata::init()
{
    qDebug() << "init"
             << (QTest::currentTestFunction() ? QTest::currentTestFunction() : "(null)")
             << (QTest::currentDataTag() ? QTest::currentDataTag() : "(null)");
}

void tst_globaldata::cleanup()
{
    qDebug() << "cleanup"
             << (QTest::currentTestFunction() ? QTest::currentTestFunction() : "(null)")
             << (QTest::currentDataTag() ? QTest::currentDataTag() : "(null)");
}

void tst_globaldata::testGlobal_data()
{
    QTest::addColumn<bool>("booll");
    QTest::newRow("local 1") << false;
    QTest::newRow("local 2") << true;
}

void tst_globaldata::testGlobal()
{
    QFETCH_GLOBAL(bool, booli);
    qDebug() << "global:" << booli;
    QFETCH(bool, booll);
    qDebug() << "local:" << booll;
}

void tst_globaldata::skip_data()
{
    QTest::addColumn<bool>("booll");
    QTest::newRow("local 1") << false;
    QTest::newRow("local 2") << true;

    QSKIP("skipping", SkipAll);
}

void tst_globaldata::skip()
{
    qDebug() << "this line should never be reached";
}

void tst_globaldata::skipSingle()
{
    QFETCH_GLOBAL(bool, booli);
    QFETCH(bool, booll);

    if (booli && !booll)
        QSKIP("skipping", SkipSingle);
    qDebug() << "global:" << booli << "local:" << booll;
}

void tst_globaldata::skipLocal()
{
    QSKIP("skipping", SkipAll);
}

QTEST_MAIN(tst_globaldata)

#include "tst_globaldata.moc"
